//=======================================================================
//			Copyright XashXT Group 2010 ©
//			gl_sprite.c - sprite rendering
//=======================================================================

#include "common.h"
#include "client.h"
#include "gl_local.h"
#include "pm_local.h"
#include "sprite.h"
#include "studio.h"
#include "entity_types.h"
#include "cl_tent.h"

// it's a Valve default value for LoadMapSprite (probably must be power of two)
#define MAPSPRITE_SIZE	128

convar_t		*r_sprite_lerping;
char		sprite_name[64];
char		group_suffix[8];
static vec3_t	sprite_mins, sprite_maxs;
static float	sprite_radius;

/*
====================
R_SpriteInit

====================
*/
void R_SpriteInit( void )
{
	r_sprite_lerping = Cvar_Get( "r_sprite_lerping", "1", CVAR_ARCHIVE, "enables sprite animation lerping" );
}

/*
====================
R_SpriteLoadFrame

upload a single frame
====================
*/
static dframetype_t *R_SpriteLoadFrame( model_t *mod, void *pin, mspriteframe_t **ppframe, int num )
{
	dspriteframe_t	*pinframe;
	mspriteframe_t	*pspriteframe;
	char		texname[64];

	// build uinque frame name
	if( !sprite_name[0] ) FS_FileBase( mod->name, sprite_name );
	com.snprintf( texname, sizeof( texname ), "#%s_%s_%i%i.spr", sprite_name, group_suffix, num / 10, num % 10 );
	
	pinframe = (dspriteframe_t *)pin;

	// setup frame description
	pspriteframe = Mem_Alloc( mod->mempool, sizeof( mspriteframe_t ));
	pspriteframe->width = pinframe->width;
	pspriteframe->height = pinframe->height;
	pspriteframe->up = pinframe->origin[1];
	pspriteframe->left = pinframe->origin[0];
	pspriteframe->down = pinframe->origin[1] - pinframe->height;
	pspriteframe->right = pinframe->width + pinframe->origin[0];
	pspriteframe->gl_texturenum = GL_LoadTexture( texname, pin, pinframe->width * pinframe->height, 0 );
	*ppframe = pspriteframe;

	GL_SetTextureType( pspriteframe->gl_texturenum, TEX_SPRITE );

	return (dframetype_t *)((byte *)(pinframe + 1) + pinframe->width * pinframe->height );
}

/*
====================
R_SpriteLoadGroup

upload a group frames
====================
*/
static dframetype_t *R_SpriteLoadGroup( model_t *mod, void *pin, mspriteframe_t **ppframe, int framenum )
{
	dspritegroup_t	*pingroup;
	mspritegroup_t	*pspritegroup;
	dspriteinterval_t	*pin_intervals;
	float		*poutintervals;
	int		i, groupsize, numframes;
	void		*ptemp;

	pingroup = (dspritegroup_t *)pin;
	numframes = pingroup->numframes;

	groupsize = sizeof( mspritegroup_t ) + (numframes - 1) * sizeof( pspritegroup->frames[0] );
	pspritegroup = Mem_Alloc( mod->mempool, groupsize );
	pspritegroup->numframes = numframes;

	*ppframe = (mspriteframe_t *)pspritegroup;
	pin_intervals = (dspriteinterval_t *)(pingroup + 1);
	poutintervals = Mem_Alloc( mod->mempool, numframes * sizeof( float ));
	pspritegroup->intervals = poutintervals;

	for( i = 0; i < numframes; i++ )
	{
		*poutintervals = pin_intervals->interval;
		if( *poutintervals <= 0.0f )
			*poutintervals = 1.0f; // set error value
		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void *)pin_intervals;
	for( i = 0; i < numframes; i++ )
	{
		ptemp = R_SpriteLoadFrame( mod, ptemp, &pspritegroup->frames[i], framenum * 10 + i );
	}

	return (dframetype_t *)ptemp;
}

/*
====================
Mod_LoadSpriteModel

load sprite model
====================
*/
void Mod_LoadSpriteModel( model_t *mod, const void *buffer )
{
	dsprite_t		*pin;
	short		*numi;
	msprite_t		*psprite;
	dframetype_t	*pframetype;
	int		i, size;

	pin = (dsprite_t *)buffer;
	i = pin->version;

	if( pin->ident != IDSPRITEHEADER )
	{
		MsgDev( D_ERROR, "%s has wrong id (%x should be %x)\n", mod->name, pin->ident, IDSPRITEHEADER );
		return;
	}
		
	if( i != SPRITE_VERSION )
	{
		MsgDev( D_ERROR, "%s has wrong version number (%i should be %i)\n", mod->name, i, SPRITE_VERSION );
		return;
	}

	mod->mempool = Mem_AllocPool( va( "^2%s^7", mod->name ));
	size = sizeof( msprite_t ) + ( pin->numframes - 1 ) * sizeof( psprite->frames );
	psprite = Mem_Alloc( mod->mempool, size );
	mod->cache.data = psprite;	// make link to extradata
	
	psprite->type = pin->type;
	psprite->texFormat = pin->texFormat;
	psprite->numframes = mod->numframes = pin->numframes;
	psprite->facecull = pin->facetype;
	psprite->radius = pin->boundingradius;
	psprite->synctype = pin->synctype;

	mod->mins[0] = mod->mins[1] = -pin->bounds[0] / 2;
	mod->maxs[0] = mod->maxs[1] = pin->bounds[0] / 2;
	mod->mins[2] = -pin->bounds[1] / 2;
	mod->maxs[2] = pin->bounds[1] / 2;
	numi = (short *)(pin + 1);

	if( *numi == 256 )
	{	
		byte	*src = (byte *)(numi+1);
		rgbdata_t	*pal;
	
		// install palette
		switch( psprite->texFormat )
		{
		case SPR_ADDITIVE:
			pal = FS_LoadImage( "#normal.pal", src, 768 );
			break;
                    case SPR_INDEXALPHA:
			pal = FS_LoadImage( "#decal.pal", src, 768 );
			break;
		case SPR_ALPHTEST:		
			pal = FS_LoadImage( "#transparent.pal", src, 768 );
                              break;
		case SPR_NORMAL:
		default:
			pal = FS_LoadImage( "#normal.pal", src, 768 );
			break;
		}

		pframetype = (dframetype_t *)(src + 768);
		FS_FreeImage( pal ); // palette installed, no reason to keep this data
	}
	else 
	{
		MsgDev( D_ERROR, "%s has wrong number of palette colors %i (should be 256)\n", mod->name, numi );
		Mem_FreePool( &mod->mempool );
		return;
	}

	if( pin->numframes < 1 )
	{
		MsgDev( D_ERROR, "%s has invalid # of frames: %d\n", mod->name, pin->numframes );
		Mem_FreePool( &mod->mempool );
		return;
	}

	// reset the sprite name
	sprite_name[0] = '\0';

	for( i = 0; i < pin->numframes; i++ )
	{
		frametype_t frametype = pframetype->type;
		psprite->frames[i].type = frametype;

		switch( frametype )
		{
		case FRAME_SINGLE:
			com.strncpy( group_suffix, "one", sizeof( group_suffix ));
			pframetype = R_SpriteLoadFrame( mod, pframetype + 1, &psprite->frames[i].frameptr, i );
			break;
		case FRAME_GROUP:
			com.strncpy( group_suffix, "grp", sizeof( group_suffix ));
			pframetype = R_SpriteLoadGroup( mod, pframetype + 1, &psprite->frames[i].frameptr, i );
			break;
		case FRAME_ANGLED:
			com.strncpy( group_suffix, "ang", sizeof( group_suffix ));
			pframetype = R_SpriteLoadGroup( mod, pframetype + 1, &psprite->frames[i].frameptr, i );
			break;
		}
		if( pframetype == NULL ) break; // technically an error
	}

	mod->type = mod_sprite; // done
}

/*
====================
Mod_LoadMapSprite

Loading a bitmap image as sprite with multiple frames
as pieces of input image
====================
*/
void Mod_LoadMapSprite( model_t *mod, const void *buffer, size_t size )
{
	byte		*src, *dst;
	rgbdata_t		*pix, temp;
	char		texname[64];
	int		i, j, x, y, w, h;
	int		xl, yl, xh, yh;
	int		linedelta, numframes;
	mspriteframe_t	*pspriteframe;
	msprite_t		*psprite;

	com.snprintf( texname, sizeof( texname ), "#%s", mod->name );
	pix = FS_LoadImage( texname, buffer, size );
	if( !pix ) return;	// bad image or something else

	if( pix->width % MAPSPRITE_SIZE )
		w = pix->width - ( pix->width % MAPSPRITE_SIZE );
	else w = pix->width;

	if( pix->height % MAPSPRITE_SIZE )
		h = pix->height - ( pix->height % MAPSPRITE_SIZE );
	else h = pix->height;

	if( w < MAPSPRITE_SIZE ) w = MAPSPRITE_SIZE;
	if( h < MAPSPRITE_SIZE ) h = MAPSPRITE_SIZE;

	// resample image if needed
	Image_Process( &pix, w, h, IMAGE_FORCE_RGBA|IMAGE_RESAMPLE );

	w = h = MAPSPRITE_SIZE;

	// check range
	if( w > pix->width ) w = pix->width;
	if( h > pix->height ) h = pix->height;

	// determine how many frames we needs
	numframes = (pix->width * pix->height) / (w * h);
	mod->mempool = Mem_AllocPool( va( "^2%s^7", mod->name ));
	psprite = Mem_Alloc( mod->mempool, sizeof( msprite_t ) + ( numframes - 1 ) * sizeof( psprite->frames ));
	mod->cache.data = psprite;	// make link to extradata

	psprite->type = SPR_FWD_PARALLEL_ORIENTED;
	psprite->texFormat = SPR_ALPHTEST;
	psprite->numframes = mod->numframes = numframes;
	psprite->radius = com.sqrt((( w >> 1) * (w >> 1)) + ((h >> 1) * (h >> 1)));

	mod->mins[0] = mod->mins[1] = -w / 2;
	mod->maxs[0] = mod->maxs[1] = w / 2;
	mod->mins[2] = -h / 2;
	mod->maxs[2] = h / 2;

	// create a temporary pic
	temp.width = w;
	temp.height = h;
	temp.type = pix->type;
	temp.flags = pix->flags;	
	temp.size = w * h * PFDesc( temp.type )->bpp;
	temp.buffer = Mem_Alloc( r_temppool, temp.size );
	temp.palette = NULL;

	// reset the sprite name
	sprite_name[0] = '\0';

	// chop the image and upload into video memory
	for( i = xl = yl = 0; i < numframes; i++ )
	{
		xh = xl + w;
		yh = yl + h;

		src = pix->buffer + ( yl * pix->width + xl ) * 4;
		linedelta = ( pix->width - w ) * 4;
		dst = temp.buffer;

		// cut block from source
		for( y = yl; y < yh; y++ )
		{
			for( x = xl; x < xh; x++ )
				for( j = 0; j < 4; j++ )
					*dst++ = *src++;
			src += linedelta;
		}

		// build uinque frame name
		if( !sprite_name[0] ) FS_FileBase( mod->name, sprite_name );
		com.snprintf( texname, sizeof( texname ), "#%s_%i%i.spr", sprite_name, i / 10, i % 10 );

		psprite->frames[i].frameptr = Mem_Alloc( mod->mempool, sizeof( mspriteframe_t ));
		pspriteframe = psprite->frames[i].frameptr;
		pspriteframe->width = w;
		pspriteframe->height = h;
		pspriteframe->up = ( h >> 1 );
		pspriteframe->left = -( w >> 1 );
		pspriteframe->down = ( h >> 1 ) - h;
		pspriteframe->right = w + -( w >> 1 );
		pspriteframe->gl_texturenum = GL_LoadTextureInternal( texname, &temp, TF_IMAGE, false );
		GL_SetTextureType( pspriteframe->gl_texturenum, TEX_NOMIP );
			
		xl += w;
		if( xl >= pix->width )
		{
			xl = 0;
			yl += h;
		}
	}

	FS_FreeImage( pix );
	Mem_Free( temp.buffer );
	mod->type = mod_sprite; // done
}

/*
====================
Mod_UnloadSpriteModel

release sprite model and frames
====================
*/
void Mod_UnloadSpriteModel( model_t *mod )
{
	msprite_t		*psprite;
	mspritegroup_t	*pspritegroup;	
	mspriteframe_t	*pspriteframe;
	int		i, j;

	ASSERT( mod != NULL );

	if( mod->type != mod_sprite )
		return; // not a sprite

	psprite = mod->cache.data;
	if( !psprite ) return; // already freed

	// release all textures
	for( i = 0; i < psprite->numframes; i++ )
	{
		if( psprite->frames[i].type == SPR_SINGLE )
		{
			pspriteframe = psprite->frames[i].frameptr;
			GL_FreeTexture( pspriteframe->gl_texturenum );
		}
		else
		{
			pspritegroup = (mspritegroup_t *)psprite->frames[i].frameptr;

			for( j = 0; j < pspritegroup->numframes; j++ )
			{
				pspriteframe = pspritegroup->frames[i];
				GL_FreeTexture( pspriteframe->gl_texturenum );
			}
		}
	}

	Mem_FreePool( &mod->mempool );
	Mem_Set( mod, 0, sizeof( *mod ));
}

/*
================
R_GetSpriteFrame

assume pModel is valid
================
*/
mspriteframe_t *R_GetSpriteFrame( const model_t *pModel, int frame, float yaw )
{
	msprite_t		*psprite;
	mspritegroup_t	*pspritegroup;
	mspriteframe_t	*pspriteframe;
	float		*pintervals, fullinterval;
	float		targettime, time;
	int		i, numframes;

	ASSERT( pModel );
	psprite = pModel->cache.data;

	if( frame < 0 ) frame = 0;
	else if( frame >= psprite->numframes )
	{
		MsgDev( D_WARN, "R_GetSpriteFrame: no such frame %d (%s)\n", frame, pModel->name );
		frame = psprite->numframes - 1;
	}

	if( psprite->frames[frame].type == SPR_SINGLE )
	{
		pspriteframe = psprite->frames[frame].frameptr;
	}
	else if( psprite->frames[frame].type == SPR_GROUP ) 
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];
		time = cl.time;

		// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
		// are positive, so we don't have to worry about division by zero
		targettime = time - ((int)(time / fullinterval)) * fullinterval;

		for( i = 0; i < (numframes - 1); i++ )
		{
			if( pintervals[i] > targettime )
				break;
		}
		pspriteframe = pspritegroup->frames[i];
	}
	else if( psprite->frames[frame].type == FRAME_ANGLED )
	{
		int	angleframe = (int)(( RI.refdef.viewangles[1] - yaw ) / 360 * 8 + 0.5 - 4) & 7;

		// e.g. doom-style sprite monsters
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pspriteframe = pspritegroup->frames[angleframe];
	}
	return pspriteframe;
}

/*
================
R_GetSpriteFrameInterpolant

NOTE: we using prevblending[0] and [1] for holds interval
between frames where are we lerping
================
*/
float R_GetSpriteFrameInterpolant( cl_entity_t *ent, mspriteframe_t **oldframe, mspriteframe_t **curframe )
{
	msprite_t		*psprite;
	mspritegroup_t	*pspritegroup;
	int		i, j, numframes, frame;
	float		lerpFrac, time, jtime, jinterval;
	float		*pintervals, fullinterval, targettime;
	int		m_fDoInterp;

	psprite = ent->model->cache.data;
	frame = (int)ent->curstate.frame;
	lerpFrac = 1.0f;

	// misc info
	if( r_sprite_lerping->integer )
		m_fDoInterp = (ent->curstate.effects & EF_NOINTERP) ? false : true;
	else m_fDoInterp = false;

	if( frame < 0 )
	{
		frame = 0;
	}          
	else if( frame >= psprite->numframes )
	{
		MsgDev( D_WARN, "R_GetSpriteFrame: no such frame %d (%s)\n", frame, ent->model->name );
		frame = psprite->numframes - 1;
	}

	if( psprite->frames[frame].type == FRAME_SINGLE )
	{
		if( m_fDoInterp )
		{
			if( ent->latched.prevblending[0] >= psprite->numframes || psprite->frames[ent->latched.prevblending[0]].type != FRAME_SINGLE )
			{
				// this can be happens when rendering switched between single and angled frames
				// or change model on replace delta-entity
				ent->latched.prevblending[0] = ent->latched.prevblending[1] = frame;
				ent->latched.prevanimtime = RI.refdef.time;
				lerpFrac = 1.0f;
			}
                              
			if( ent->latched.prevanimtime < RI.refdef.time )
			{
				if( frame != ent->latched.prevblending[1] )
				{
					ent->latched.prevblending[0] = ent->latched.prevblending[1];
					ent->latched.prevblending[1] = frame;
					ent->latched.prevanimtime = RI.refdef.time;
					lerpFrac = 0.0f;
				}
				else lerpFrac = (RI.refdef.time - ent->latched.prevanimtime) * 10;
			}
			else
			{
				ent->latched.prevblending[0] = ent->latched.prevblending[1] = frame;
				ent->latched.prevanimtime = RI.refdef.time;
				lerpFrac = 0.0f;
			}
		}
		else
		{
			ent->latched.prevblending[0] = ent->latched.prevblending[1] = frame;
			lerpFrac = 1.0f;
		}

		if( ent->latched.prevblending[0] >= psprite->numframes )
		{
			// reset interpolation on change model
			ent->latched.prevblending[0] = ent->latched.prevblending[1] = frame;
			ent->latched.prevanimtime = RI.refdef.time;
			lerpFrac = 0.0f;
		}

		// get the interpolated frames
		if( oldframe ) *oldframe = psprite->frames[ent->latched.prevblending[0]].frameptr;
		if( curframe ) *curframe = psprite->frames[frame].frameptr;
	}
	else if( psprite->frames[frame].type == FRAME_GROUP ) 
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];
		jinterval = pintervals[1] - pintervals[0];
		time = RI.refdef.time;
		jtime = 0.0f;

		// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
		// are positive, so we don't have to worry about division by zero
		targettime = time - ((int)(time / fullinterval)) * fullinterval;

		// LordHavoc: since I can't measure the time properly when it loops from numframes - 1 to 0,
		// i instead measure the time of the first frame, hoping it is consistent
		for( i = 0, j = numframes - 1; i < (numframes - 1); i++ )
		{
			if( pintervals[i] > targettime )
				break;
			j = i;
			jinterval = pintervals[i] - jtime;
			jtime = pintervals[i];
		}

		if( m_fDoInterp )
			lerpFrac = (targettime - jtime) / jinterval;
		else j = i; // no lerping

		// get the interpolated frames
		if( oldframe ) *oldframe = pspritegroup->frames[j];
		if( curframe ) *curframe = pspritegroup->frames[i];
	}
	else if( psprite->frames[frame].type == FRAME_ANGLED )
	{
		// e.g. doom-style sprite monsters
		float	yaw = ent->angles[YAW];
		int	angleframe = (int)(( RI.refdef.viewangles[1] - yaw ) / 360 * 8 + 0.5 - 4) & 7;

		if( m_fDoInterp )
		{
			if( ent->latched.prevblending[0] >= psprite->numframes || psprite->frames[ent->latched.prevblending[0]].type != FRAME_ANGLED )
			{
				// this can be happens when rendering switched between single and angled frames
				// or change model on replace delta-entity
				ent->latched.prevblending[0] = ent->latched.prevblending[1] = frame;
				ent->latched.prevanimtime = RI.refdef.time;
				lerpFrac = 1.0f;
			}

			if( ent->latched.prevanimtime < RI.refdef.time )
			{
				if( frame != ent->latched.prevblending[1] )
				{
					ent->latched.prevblending[0] = ent->latched.prevblending[1];
					ent->latched.prevblending[1] = frame;
					ent->latched.prevanimtime = RI.refdef.time;
					lerpFrac = 0.0f;
				}
				else lerpFrac = (RI.refdef.time - ent->latched.prevanimtime) * ent->curstate.framerate;
			}
			else
			{
				ent->latched.prevblending[0] = ent->latched.prevblending[1] = frame;
				ent->latched.prevanimtime = RI.refdef.time;
				lerpFrac = 0.0f;
			}
		}
		else
		{
			ent->latched.prevblending[0] = ent->latched.prevblending[1] = frame;
			lerpFrac = 1.0f;
		}

		pspritegroup = (mspritegroup_t *)psprite->frames[ent->latched.prevblending[0]].frameptr;
		if( oldframe ) *oldframe = pspritegroup->frames[angleframe];

		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		if( curframe ) *curframe = pspritegroup->frames[angleframe];
	}
	return lerpFrac;
}

/*
================
R_StudioComputeBBox

Compute a full bounding box for current sequence
================
*/
qboolean R_SpriteComputeBBox( cl_entity_t *e, vec3_t bbox[8] )
{
	float	scale = 1.0f;
	vec3_t	p1;
	int	i;

	// copy original bbox (no rotation for sprites)
	VectorCopy( e->model->mins, sprite_mins );
	VectorCopy( e->model->maxs, sprite_maxs );

	// compute a full bounding box
	for( i = 0; bbox && i < 8; i++ )
	{
  		p1[0] = ( i & 1 ) ? sprite_mins[0] : sprite_maxs[0];
  		p1[1] = ( i & 2 ) ? sprite_mins[1] : sprite_maxs[1];
  		p1[2] = ( i & 4 ) ? sprite_mins[2] : sprite_maxs[2];

		VectorCopy( p1, bbox[i] );
	}

	if( e->curstate.scale > 0.0f )
		scale = e->curstate.scale;

	sprite_radius = RadiusFromBounds( sprite_mins, sprite_maxs ) * scale;

	return true;
}

/*
================
R_CullSpriteModel

Cull sprite model by bbox
================
*/
qboolean R_CullSpriteModel( cl_entity_t *e )
{
	if( !e->model->cache.data )
		return true;

	if( e == &clgame.viewent && r_lefthand->integer >= 2 )
		return true;

	if( !R_SpriteComputeBBox( e, NULL ))
		return true; // invalid frame

	return R_CullModel( e, sprite_mins, sprite_maxs, sprite_radius );
}

/*
================
R_GlowSightDistance

Calc sight distance for glow-sprites
================
*/
static float R_GlowSightDistance( vec3_t glowOrigin )
{
	float	dist;
	vec3_t	glowDist;
	pmtrace_t	tr;

	VectorSubtract( glowOrigin, RI.vieworg, glowDist );
	dist = VectorLength( glowDist );

	tr = PM_PlayerTrace( clgame.pmove, RI.vieworg, glowOrigin, PM_GLASS_IGNORE|PM_STUDIO_IGNORE, 2, -1, NULL );

	if(( 1.0f - tr.fraction ) * dist > 8 )
		return -1;
	return dist;
}

/*
================
R_GlowSightDistance

Set sprite brightness factor
================
*/
static float R_SpriteGlowBlend( cl_entity_t *e, vec3_t origin )
{
	float	dist = R_GlowSightDistance( origin );
	float	brightness;

	if( dist <= 0 ) return 0.0f; // occluded

	if( e->curstate.renderfx == kRenderFxNoDissipation )
		return (float)e->curstate.renderamt * (1.0f / 255.0f);

	// UNDONE: Tweak these magic numbers (19000 - falloff & 200 - sprite size)
	brightness = 19000.0 / ( dist * dist );
	brightness = bound( 0.05f, brightness, 1.0f );

	// Make the glow fixed size in screen space, taking into consideration the scale setting.
	if( e->curstate.scale == 0.0f ) e->curstate.scale = 1.0f;
	e->curstate.scale *= dist * ( 1.0f / 200.0f );

	return brightness;
}

/*
================
R_SpriteOccluded

Do occlusion test for glow-sprites
================
*/
qboolean R_SpriteOccluded( cl_entity_t *e, vec3_t origin )
{
	if( e->curstate.rendermode == kRenderGlow )
	{
		float	blend = 1.0f;
		vec3_t	v;

		TriWorldToScreen( origin, v );

		if( v[0] < RI.refdef.viewport[0] || v[0] > RI.refdef.viewport[0] + RI.refdef.viewport[2] )
			return true; // do scissor
		if( v[1] < RI.refdef.viewport[1] || v[1] > RI.refdef.viewport[1] + RI.refdef.viewport[3] )
			return true; // do scissor

		blend *= R_SpriteGlowBlend( e, origin );
		e->curstate.renderamt *= blend;

		if( blend <= 0.05f )
			return true; // faded
	}
	else
	{
		if( R_CullSpriteModel( e ))
			return true;
	}
	return false;	
}

/*
=================
R_DrawSpriteQuad
=================
*/
static void R_DrawSpriteQuad( mspriteframe_t *frame, vec3_t org, vec3_t v_right, vec3_t v_up, float scale )
{
	vec3_t	point;

	GL_Bind( GL_TEXTURE0, frame->gl_texturenum );

	pglBegin( GL_QUADS );
		pglTexCoord2f( 0.0f, 1.0f );
		VectorMA( org, frame->down * scale, v_up, point );
		VectorMA( point, frame->left * scale, v_right, point );
		pglVertex3fv( point );
		pglTexCoord2f( 0.0f, 0.0f );
		VectorMA( org, frame->up * scale, v_up, point );
		VectorMA( point, frame->left * scale, v_right, point );
		pglVertex3fv( point );
		pglTexCoord2f( 1.0f, 0.0f );
		VectorMA( org, frame->up * scale, v_up, point );
		VectorMA( point, frame->right * scale, v_right, point );
		pglVertex3fv( point );
 	        	pglTexCoord2f( 1.0f, 1.0f );
		VectorMA( org, frame->down * scale, v_up, point );
		VectorMA( point, frame->right * scale, v_right, point );
		pglVertex3fv( point );
	pglEnd();
}

/*
=================
R_DrawSpriteModel
=================
*/
void R_DrawSpriteModel( cl_entity_t *e )
{
	mspriteframe_t	*frame, *oldframe;
	msprite_t		*psprite;
	model_t		*model;
	int		i, state = 0;
	float		angle, sr, cr;
	float		lerp = 1.0f, ilerp;
	vec3_t		v_forward, v_right, v_up;
	vec3_t		origin;
	color24		color;

	model = e->model;
	psprite = (msprite_t * )model->cache.data;
	color.r = color.g = color.b = 255;

	VectorCopy( e->origin, origin );	// set render origin

	// do movewith
	if( e->curstate.aiment > 0 && e->curstate.movetype == MOVETYPE_FOLLOW )
	{
		cl_entity_t	*parent;
	
		parent = CL_GetEntityByIndex( e->curstate.aiment );

		if( parent && parent->model && parent->model->type == mod_studio )
		{
			if( e->curstate.body > 0 )
			{
				int num = bound( 1, e->curstate.body, MAXSTUDIOATTACHMENTS );
				VectorCopy( parent->attachment[num-1], origin );
			}
			else VectorCopy( parent->origin, origin );
		}
	}

	if( R_SpriteOccluded( e, origin ))
		return; // sprite culled

	r_stats.c_sprite_models_drawn++;

	if( psprite->texFormat == SPR_ALPHTEST )
		state |= GLSTATE_AFUNC_GE128|GLSTATE_DEPTHWRITE;

	// select properly rendermode
	switch( e->curstate.rendermode )
	{
	case kRenderTransAlpha:
	case kRenderTransTexture:
		state |= GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA;
		break;
	case kRenderGlow:
	case kRenderTransAdd:
		state |= GLSTATE_SRCBLEND_SRC_ALPHA|GLSTATE_DSTBLEND_ONE;
		break;
	case kRenderNormal:
	default:	state |= GLSTATE_DEPTHWRITE;
		break;
	}

	if( e->curstate.rendermode == kRenderNormal )
		GL_TexEnv( GL_REPLACE );
	else GL_TexEnv( GL_MODULATE );

	GL_SetState( state );

	if( psprite->texFormat == SPR_ALPHTEST )
	{
		vec3_t	brightness;

		R_LightForPoint( origin, &color, false, sprite_radius );
		brightness[0] = color.r * ( 1.0f / 255.0f );
		brightness[1] = color.g * ( 1.0f / 255.0f );
		brightness[2] = color.b * ( 1.0f / 255.0f );

		color.r = e->curstate.rendercolor.r * brightness[0];
		color.g = e->curstate.rendercolor.g * brightness[1];
		color.b = e->curstate.rendercolor.b * brightness[2];
	}
	else
	{
		color.r = e->curstate.rendercolor.r;
		color.g = e->curstate.rendercolor.g;
		color.b = e->curstate.rendercolor.b;
	}

	if( e->curstate.rendermode == kRenderNormal || e->curstate.rendermode == kRenderTransAlpha )
		frame = oldframe = R_GetSpriteFrame( model, e->curstate.frame, e->angles[YAW] );
	else lerp = R_GetSpriteFrameInterpolant( e, &oldframe, &frame );

	switch( psprite->type )
	{
	case SPR_ORIENTED:
		AngleVectors( e->angles, v_forward, v_right, v_up );
		VectorScale( v_forward, 0.01f, v_forward ); // to avoid z-fighting
		VectorSubtract( origin, v_forward, origin );
		break;
	case SPR_FACING_UPRIGHT:
		VectorSet( v_right, origin[1] - RI.vieworg[1], -(origin[0] - RI.vieworg[0]), 0.0f );
		VectorSet( v_up, 0.0f, 0.0f, 1.0f );
		VectorNormalize( v_right );
		break;
	case SPR_FWD_PARALLEL_UPRIGHT:
		VectorSet( v_right, RI.vforward[1], -RI.vforward[0], 0.0f );
		VectorSet( v_up, 0.0f, 0.0f, 1.0f );
		break;
	case SPR_FWD_PARALLEL_ORIENTED:
		angle = e->angles[ROLL] * (M_PI * 2.0f / 360.0f);
		com.sincos( angle, &sr, &cr );
		for( i = 0; i < 3; i++ )
		{
			v_right[i] = (RI.vright[i] * cr + RI.vup[i] * sr);
			v_up[i] = RI.vright[i] * -sr + RI.vup[i] * cr;
		}
		break;
	case SPR_FWD_PARALLEL: // normal sprite
	default:
		if( e->curstate.entityType == ET_TEMPENTITY )
			angle = e->angles[ROLL]; // for support rotating muzzleflashes
		else angle = 0.0f;

		if( angle != 0.0f )
		{
			RotatePointAroundVector( v_up, RI.vforward, RI.vright, angle );	// make up
			CrossProduct( RI.vforward, v_up, v_right );			// make right
		}
		else
		{
			VectorCopy( RI.vright, v_right ); 
			VectorCopy( RI.vup, v_up );
		}
		break;
	}

	if( e->curstate.rendermode == kRenderGlow )
		pglDepthRange( gldepthmin, gldepthmin + 0.3f * ( gldepthmax - gldepthmin ));

	if( psprite->facecull == SPR_CULL_NONE )
		GL_Cull( GL_NONE );
		
	if( oldframe == frame )
	{
		// draw the single non-lerped frame
		pglColor4ub( color.r, color.g, color.b, e->curstate.renderamt );
		R_DrawSpriteQuad( frame, origin, v_right, v_up, e->curstate.scale );
	}
	else
	{
		// draw two combined lerped frames
		lerp = bound( 0.0f, lerp, 1.0f );
		ilerp = 1.0f - lerp;

		if( ilerp != 0 )
		{
			pglColor4ub( color.r, color.g, color.b, e->curstate.renderamt * ilerp );
			R_DrawSpriteQuad( oldframe, origin, v_right, v_up, e->curstate.scale );
		}

		if( lerp != 0 )
		{
			pglColor4ub( color.r, color.g, color.b, e->curstate.renderamt * lerp );
			R_DrawSpriteQuad( frame, origin, v_right, v_up, e->curstate.scale );
		}
	}

	if( e->curstate.rendermode == kRenderGlow )
		pglDepthRange( gldepthmin, gldepthmax );

	if( psprite->facecull == SPR_CULL_NONE )
		GL_Cull( GL_FRONT );

	pglColor4ub( 255, 255, 255, 255 );
}