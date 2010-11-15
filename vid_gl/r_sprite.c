//=======================================================================
//			Copyright XashXT Group 2007 ©
//		        r_sprite.c - render sprite models
//=======================================================================

#include "r_local.h"
#include "mathlib.h"
#include "byteorder.h"
#include "const.h"
#include "cl_entity.h"

/*
=============================================================

  SPRITE MODELS

=============================================================
*/
string		frame_prefix;
uint		frame_type;
uint		group_num;
string		sp_name;
ref_shader_t	**frames = NULL;
convar_t		*r_sprite_lerping;
uint		tex_flags = 0;
vec3_t		sprite_mins, sprite_maxs;
float		sprite_radius;

/*
====================
R_SpriteInit

====================
*/
void R_SpriteInit( void )
{
	r_sprite_lerping = Cvar_Get( "r_sprite_lerping", "1", CVAR_ARCHIVE, "enables sprite model animation lerping" );
}
	
/*
====================
Sprite model loader
====================
*/
static dframetype_t *R_SpriteLoadFrame( ref_model_t *mod, void *pin, mspriteframe_t **ppframe, int framenum )
{
	texture_t		*tex;
	dspriteframe_t	*pinframe;
	mspriteframe_t	*pspriteframe;
	string		name, shadername;

	// build uinque frame name
	if( !sp_name[0] ) FS_FileBase( mod->name, sp_name );
	com.snprintf( name, MAX_STRING, "Sprite( %s_%s_%i%i )", sp_name, frame_prefix, framenum/10, framenum%10 );
	
	pinframe = (dspriteframe_t *)pin;
	SwapBlock((int *)pinframe, sizeof( dspriteframe_t ));

	// setup frame description
	pspriteframe = Mem_Alloc( mod->mempool, sizeof( mspriteframe_t ));
	pspriteframe->width = pinframe->width;
	pspriteframe->height = pinframe->height;
	pspriteframe->up = pinframe->origin[1];
	pspriteframe->left = pinframe->origin[0];
	pspriteframe->down = pinframe->origin[1] - pinframe->height;
	pspriteframe->right = pinframe->width + pinframe->origin[0];
	pspriteframe->radius = com.sqrt((pinframe->width * pinframe->width) + (pinframe->height * pinframe->height));
	tex = R_FindTexture( name, (byte *)pin, pinframe->width * pinframe->height, 0 );
	tex_flags |= tex->flags;
	*ppframe = pspriteframe;

	R_ShaderAddStageTexture( tex );

	if( frame_type == FRAME_SINGLE )
	{
		com.snprintf( shadername, MAX_STRING, "sprites/%s.spr/%s_%i%i )", sp_name, frame_prefix, framenum/10, framenum%10 );
		pspriteframe->shader = R_LoadShader( shadername, SHADER_SPRITE, true, tex_flags, SHADER_INVALID )->shadernum;
		frames = Mem_Realloc( mod->mempool, frames, sizeof( ref_shader_t* ) * (mod->numshaders + 1));
		frames[mod->numshaders++] = &r_shaders[pspriteframe->shader];
	}
	return (dframetype_t *)((byte *)(pinframe + 1) + pinframe->width * pinframe->height );
}

static dframetype_t *CL_LoadSpriteFrame( const char *szSpriteName, void *pin, int framenum )
{
	texture_t		*tex;
	string		name;
	dspriteframe_t	*pinframe;

	// build uinque frame name
	if( !sp_name[0] ) FS_FileBase( szSpriteName, sp_name );
	com.snprintf( name, MAX_STRING, "Sprite( \"%s_%s_%i%i\" )", sp_name, frame_prefix, framenum/10, framenum%10 );

	pinframe = (dspriteframe_t *)pin;	
	SwapBlock((int *)pinframe, sizeof( dspriteframe_t ));

	// NOTE: just loading all single frame into one shader
	// we supported only single frames in this case
	// because we can't properly merge multiple groups into single shader
	tex = R_FindTexture( name, (byte *)pin, pinframe->width * pinframe->height, tex_flags );
	R_ShaderAddStageTexture( tex );

	return (dframetype_t *)((byte *)(pinframe + 1) + pinframe->width * pinframe->height );
}

static dframetype_t *CL_SpriteSkipGroup( const char *szSpriteName, void *pin, int framenum )
{
	dspritegroup_t	*pingroup;
	dspriteinterval_t	*pin_intervals;
	dspriteframe_t	*pinframe;
	void		*ptemp;
	int		i;

	// throw warning
	MsgDev( D_WARN, "CL_LoadSprite: %s ignore group frame %i\n", szSpriteName, framenum );

	pingroup = (dspritegroup_t *)pin;
	pin_intervals = (dspriteinterval_t *)(pingroup + 1);

	// skip intervals
	for( i = 0; i < LittleLong( pingroup->numframes ); i++ )
		pin_intervals++;

	// skip group frames
	ptemp = (void *)pin_intervals;
	for( i = 0; i < LittleLong( pingroup->numframes ); i++ )
	{
		pinframe = (dspriteframe_t *)ptemp;
		SwapBlock((int *)pinframe, sizeof( dspriteframe_t ));
		ptemp = (dframetype_t *)((byte *)(pinframe + 1) + pinframe->width * pinframe->height );
	}
	return (dframetype_t *)ptemp;
}

static dframetype_t *R_SpriteLoadGroup( ref_model_t *mod, void * pin, mspriteframe_t **ppframe, int framenum )
{
	dspritegroup_t	*pingroup;
	mspritegroup_t	*pspritegroup;
	dspriteinterval_t	*pin_intervals;
	float		*poutintervals;
	shader_t		group_shader;
	int		i, groupsize, numframes;
	string		shadername;
	void		*ptemp;

	pingroup = (dspritegroup_t *)pin;
	numframes = LittleLong( pingroup->numframes );

	groupsize = sizeof(mspritegroup_t) + (numframes - 1) * sizeof( pspritegroup->frames[0] );
	pspritegroup = Mem_Alloc( mod->mempool, groupsize );
	pspritegroup->numframes = numframes;

	*ppframe = (mspriteframe_t *)pspritegroup;
	pin_intervals = (dspriteinterval_t *)(pingroup + 1);
	poutintervals = Mem_Alloc( mod->mempool, numframes * sizeof( float ));
	pspritegroup->intervals = poutintervals;

	for( i = 0; i < numframes; i++ )
	{
		*poutintervals = LittleFloat( pin_intervals->interval );
		if( *poutintervals <= 0.0 ) *poutintervals = 1.0f; // set error value
		if( frame_type == FRAME_GROUP ) R_ShaderAddStageIntervals( *poutintervals );
		else if( frame_type == FRAME_ANGLED ) R_ShaderAddStageIntervals( -1.0f );
		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void *)pin_intervals;
	for( i = 0; i < numframes; i++ )
	{
		ptemp = R_SpriteLoadFrame( mod, ptemp, &pspritegroup->frames[i], framenum * 10 + i );
	}

	com.snprintf( shadername, MAX_STRING, "sprites/%s.spr/%s_%i%i", sp_name, frame_prefix, group_num/10, group_num%10 );
	group_shader = R_LoadShader( shadername, SHADER_SPRITE, true, tex_flags, SHADER_INVALID )->shadernum;
	frames = Mem_Realloc( mod->mempool, frames, sizeof( ref_shader_t* ) * (mod->numshaders + 1));
	frames[mod->numshaders++] = &r_shaders[group_shader];

	// apply this shader for all frames in group
	for( i = 0; i < numframes; i++ )
		pspritegroup->frames[i]->shader = group_shader;
	group_num++;

	return (dframetype_t *)ptemp;
}

void Mod_SpriteLoadModel( ref_model_t *mod, const void *buffer )
{
	dsprite_t		*pin;
	short		*numi;
	msprite_t		*psprite;
	dframetype_t	*pframetype;
	int		i, size, numframes;
	qboolean		twoSided;

	pin = (dsprite_t *)buffer;
	i = LittleLong( pin->version );
		
	if( i != SPRITE_VERSION )
	{
		MsgDev( D_ERROR, "%s has wrong version number (%i should be %i)\n", mod->name, i, SPRITE_VERSION );
		return;
	}

	numframes = LittleLong( pin->numframes );
	size = sizeof (msprite_t) + (numframes - 1) * sizeof( psprite->frames );

	psprite = Mem_Alloc( mod->mempool, size );
	mod->extradata = psprite;	// make link to extradata
	mod->numshaders = 0;	// reset frames
	
	psprite->type = LittleLong( pin->type );
	psprite->rendermode = LittleLong( pin->texFormat );
	psprite->numframes = numframes;
	twoSided = (LittleLong( pin->facetype == SPR_CULL_NONE )) ? true : false;
	mod->mins[0] = mod->mins[1] = -LittleLong( pin->bounds[0] ) / 2;
	mod->maxs[0] = mod->maxs[1] = LittleLong( pin->bounds[0] ) / 2;
	mod->mins[2] = -LittleLong( pin->bounds[1] ) / 2;
	mod->maxs[2] = LittleLong( pin->bounds[1] ) / 2;
	numi = (short *)(pin + 1);

	if( LittleShort( *numi ) == 256 )
	{	
		byte	*src = (byte *)(numi+1);
		rgbdata_t	*pal;
	
		// install palette
		switch( psprite->rendermode )
		{
		case SPR_ADDGLOW:
			pal = FS_LoadImage( "#normal.pal", src, 768 );
			R_ShaderSetRenderMode( kRenderGlow, twoSided );
			break;
		case SPR_ADDITIVE:
			pal = FS_LoadImage( "#normal.pal", src, 768 );
			R_ShaderSetRenderMode( kRenderTransAdd, twoSided );
			break;
                    case SPR_INDEXALPHA:
			pal = FS_LoadImage( "#decal.pal", src, 768 );
			R_ShaderSetRenderMode( kRenderTransTexture, twoSided );
			break;
		case SPR_ALPHTEST:		
			pal = FS_LoadImage( "#transparent.pal", src, 768 );
			R_ShaderSetRenderMode( kRenderTransAlpha, twoSided );
                              break;
		case SPR_NORMAL:
		default:
			pal = FS_LoadImage( "#normal.pal", src, 768 );
			R_ShaderSetRenderMode( kRenderNormal, twoSided );
			break;
		}
		pframetype = (dframetype_t *)(src + 768);
		FS_FreeImage( pal ); // palette installed, no reason to keep this data
	}
	else 
	{
		MsgDev( D_ERROR, "%s has wrong number of palette colors %i (should be 256)\n", mod->name, numi );
		return;
	}

	if( numframes < 1 )
	{
		MsgDev( D_ERROR, "%s has invalid # of frames: %d\n", mod->name, numframes );
		return;
	}

	mod->touchFrame = tr.registration_sequence;
	frames = NULL; // invalidate pointer
	sp_name[0] = 0;
	group_num = 0;

	for( i = 0; i < numframes; i++ )
	{
		frametype_t frametype = LittleLong( pframetype->type );
		psprite->frames[i].type = frametype;
		frame_type = frametype;
			
		switch( frametype )
		{
		case FRAME_SINGLE:
			tex_flags = 0;
			com.strncpy( frame_prefix, "one", MAX_STRING );
			pframetype = R_SpriteLoadFrame( mod, pframetype + 1, &psprite->frames[i].frameptr, i );
			break;
		case FRAME_GROUP:
			tex_flags = 0;
			com.strncpy( frame_prefix, "grp", MAX_STRING );
			pframetype = R_SpriteLoadGroup( mod, pframetype + 1, &psprite->frames[i].frameptr, i );
			break;
		case FRAME_ANGLED:
			tex_flags = 0;
			com.strncpy( frame_prefix, "ang", MAX_STRING );
			pframetype = R_SpriteLoadGroup( mod, pframetype + 1, &psprite->frames[i].frameptr, i );
			break;
		}
		if( pframetype == NULL ) break; // technically an error
	}
	mod->shaders = frames; // setup texture links
	mod->type = mod_sprite;
}

/*
================
CL_LoadSprite

client version for loading sprites
and convert them to shaders with multiple frames
================
*/
ref_shader_t *CL_LoadSprite( const char *szSpriteName )
{
	dsprite_t		*pin;
	ref_shader_t	*shader;
	byte		*buffer;
	dframetype_t	*pframetype;
	qboolean		twoSided;
	int		i, numframes;
	short		*numi;

	if( !szSpriteName || !szSpriteName[0] )
		return NULL;

	shader = R_FindShader( szSpriteName, SHADER_SPRITE, SHADER_INVALID );
	if( shader ) return shader;	// already loaded

	buffer = FS_LoadFile( szSpriteName, NULL );
	if( !buffer ) return NULL;	// sprite is missed

	pin = (dsprite_t *)buffer;

	// make sure what is really sprite
	if( LittleLong( pin->ident ) != IDSPRITEHEADER )
	{
		MsgDev( D_ERROR, "CL_LoadSprite: %s not a sprite\n", szSpriteName );
		Mem_Free( buffer );
		return NULL;
	} 

	if( LittleLong( pin->version ) != SPRITE_VERSION )
	{
		MsgDev( D_ERROR, "CL_LoadSprite: %s invalid sprite version\n", szSpriteName );
		Mem_Free( buffer );
		return NULL;
	}

	numframes = LittleLong( pin->numframes );
	twoSided = (LittleLong( pin->facetype == SPR_CULL_NONE )) ? true : false;
	numi = (short *)( pin + 1 );

	if( LittleShort( *numi ) == 256 )
	{	
		byte	*src = (byte *)(numi+1);
		rgbdata_t	*pal;
	
		// install palette
		switch( LittleLong( pin->texFormat ))
		{
		case SPR_ADDGLOW:
			pal = FS_LoadImage( "#normal.pal", src, 768 );
			R_ShaderSetRenderMode( kRenderGlow, twoSided );
			break;
		case SPR_ADDITIVE:
			pal = FS_LoadImage( "#normal.pal", src, 768 );
			R_ShaderSetRenderMode( kRenderTransAdd, twoSided );
			break;
                    case SPR_INDEXALPHA:
			pal = FS_LoadImage( "#decal.pal", src, 768 );
			R_ShaderSetRenderMode( kRenderTransTexture, twoSided );
			break;
		case SPR_ALPHTEST:		
			pal = FS_LoadImage( "#transparent.pal", src, 768 );
			R_ShaderSetRenderMode( kRenderTransAlpha, twoSided );
                              break;
		case SPR_NORMAL:
		default:
			pal = FS_LoadImage( "#normal.pal", src, 768 );
			R_ShaderSetRenderMode( kRenderNormal, twoSided );
			break;
		}
		pframetype = (dframetype_t *)(src + 768);
		FS_FreeImage( pal ); // palette installed, no reason to keep this data
	}
	else 
	{
		MsgDev( D_ERROR, "CL_LoadSprite: %s has invalid palette\n", szSpriteName );
		Mem_Free( buffer );
		return NULL;
	}

	if( numframes < 1 )
	{
		MsgDev( D_ERROR, "CL_LoadSprite: %s has invalid # of frames: %d\n", szSpriteName, numframes );
		Mem_Free( buffer );
		return NULL;
	}

	com.strncpy( frame_prefix, "one", MAX_STRING );
	tex_flags = TF_CLAMP|TF_NOMIPMAP|TF_NOPICMIP;
	frames = NULL; // invalidate pointer
	sp_name[0] = 0;
	group_num = 0;

	for( i = 0; i < numframes; i++ )
	{
		switch( LittleLong( pframetype->type ))
		{
		case FRAME_SINGLE:
			pframetype = CL_LoadSpriteFrame( szSpriteName, pframetype + 1, i );
			break;
		case FRAME_GROUP:
		case FRAME_ANGLED:
			pframetype = CL_SpriteSkipGroup( szSpriteName, pframetype + 1, i );
			break;
		}
		if( pframetype == NULL ) break; // technically an error
	}

	// all frames uploaded, release buffer
	Mem_Free( buffer );

	return R_LoadShader( szSpriteName, SHADER_SPRITE, true, tex_flags, SHADER_INVALID );
}

/*
================
R_GetSpriteFrame

assume pModel is valid
================
*/
mspriteframe_t *R_GetSpriteFrame( ref_model_t *pModel, int frame, float yawAngle )
{
	msprite_t		*psprite;
	int		i, numframes;
	mspritegroup_t	*pspritegroup;
	mspriteframe_t	*pspriteframe;
	float		*pintervals, fullinterval, targettime, time;

	ASSERT( pModel );
	psprite = pModel->extradata;

	if( frame < 0 )
	{
		frame = 0;
	}          
	else if( frame >= psprite->numframes )
	{
		MsgDev( D_WARN, "R_GetSpriteFrame: no such frame %d (%s)\n", frame, pModel->name );
		frame = psprite->numframes - 1;
	}

	if( psprite->frames[frame].type == FRAME_SINGLE )
	{
		pspriteframe = psprite->frames[frame].frameptr;
	}
	else if( psprite->frames[frame].type == FRAME_GROUP ) 
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];
		time = RI.refdef.time;

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
		// e.g. doom-style sprite monsters
		float	yaw = yawAngle - 45;	// angled bias
		int	angleframe = (int)(( RI.refdef.viewangles[1] - yaw ) / 360 * 8 + 0.5 - 4) & 7;

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
float R_GetSpriteFrameInterpolant( ref_entity_t *ent, mspriteframe_t **oldframe, mspriteframe_t **curframe )
{
	msprite_t		*psprite;
	mspritegroup_t	*pspritegroup;
	int		i, j, numframes, frame;
	float		lerpFrac, time, jtime, jinterval;
	float		*pintervals, fullinterval, targettime;
	int		m_fDoInterp;

	psprite = ent->model->extradata;
	frame = (int)ent->lerp->curstate.frame;
	lerpFrac = 1.0f;

	// misc info
	if( r_sprite_lerping->integer )
		m_fDoInterp = (ent->flags & EF_NOINTERP) ? false : true;
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
			if( ent->lerp->latched.prevblending[0] >= psprite->numframes || psprite->frames[ent->lerp->latched.prevblending[0]].type != FRAME_SINGLE )
			{
				// this can be happens when rendering switched between single and angled frames
				// or change model on replace delta-entity
				ent->lerp->latched.prevblending[0] = ent->lerp->latched.prevblending[1] = frame;
				ent->lerp->latched.prevanimtime = RI.refdef.time;
				lerpFrac = 1.0f;
			}
                              
			if( ent->lerp->latched.prevanimtime < RI.refdef.time )
			{
				if( frame != ent->lerp->latched.prevblending[1] )
				{
					ent->lerp->latched.prevblending[0] = ent->lerp->latched.prevblending[1];
					ent->lerp->latched.prevblending[1] = frame;
					ent->lerp->latched.prevanimtime = RI.refdef.time;
					lerpFrac = 0.0f;
				}
				else lerpFrac = (RI.refdef.time - ent->lerp->latched.prevanimtime) * ent->framerate;
			}
			else
			{
				ent->lerp->latched.prevblending[0] = ent->lerp->latched.prevblending[1] = frame;
				ent->lerp->latched.prevanimtime = RI.refdef.time;
				lerpFrac = 0.0f;
			}
		}
		else
		{
			ent->lerp->latched.prevblending[0] = ent->lerp->latched.prevblending[1] = frame;
			lerpFrac = 1.0f;
		}

		if( ent->lerp->latched.prevblending[0] >= psprite->numframes )
		{
			// reset interpolation on change model
			ent->lerp->latched.prevblending[0] = ent->lerp->latched.prevblending[1] = frame;
			ent->lerp->latched.prevanimtime = RI.refdef.time;
			lerpFrac = 0.0f;
		}

		// get the interpolated frames
		if( oldframe ) *oldframe = psprite->frames[ent->lerp->latched.prevblending[0]].frameptr;
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
		float	yaw = ent->angles[1] - 45;	// angled bias
		int	angleframe = (int)((RI.refdef.viewangles[1] - yaw) / 360 * 8 + 0.5 - 4) & 7;

		if( m_fDoInterp )
		{
			if( ent->lerp->latched.prevblending[0] >= psprite->numframes || psprite->frames[ent->lerp->latched.prevblending[0]].type != FRAME_ANGLED )
			{
				// this can be happens when rendering switched between single and angled frames
				// or change model on replace delta-entity
				ent->lerp->latched.prevblending[0] = ent->lerp->latched.prevblending[1] = frame;
				ent->lerp->latched.prevanimtime = RI.refdef.time;
				lerpFrac = 1.0f;
			}

			if( ent->lerp->latched.prevanimtime < RI.refdef.time )
			{
				if( frame != ent->lerp->latched.prevblending[1] )
				{
					ent->lerp->latched.prevblending[0] = ent->lerp->latched.prevblending[1];
					ent->lerp->latched.prevblending[1] = frame;
					ent->lerp->latched.prevanimtime = RI.refdef.time;
					lerpFrac = 0.0f;
				}
				else lerpFrac = (RI.refdef.time - ent->lerp->latched.prevanimtime) * ent->framerate;
			}
			else
			{
				ent->lerp->latched.prevblending[0] = ent->lerp->latched.prevblending[1] = frame;
				ent->lerp->latched.prevanimtime = RI.refdef.time;
				lerpFrac = 0.0f;
			}
		}
		else
		{
			ent->lerp->latched.prevblending[0] = ent->lerp->latched.prevblending[1] = frame;
			lerpFrac = 1.0f;
		}

		pspritegroup = (mspritegroup_t *)psprite->frames[ent->lerp->latched.prevblending[0]].frameptr;
		if( oldframe ) *oldframe = pspritegroup->frames[angleframe];

		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		if( curframe ) *curframe = pspritegroup->frames[angleframe];
	}
	return lerpFrac;
}

static float R_GlowSightDistance( vec3_t glowOrigin )
{
	float	dist;
	vec3_t	glowDist;
	pmtrace_t	tr;

	VectorSubtract( glowOrigin, RI.viewOrigin, glowDist );
	dist = VectorLength( glowDist );
	
	R_TraceLine( &tr, RI.viewOrigin, glowOrigin, FTRACE_IGNORE_GLASS|FTRACE_SIMPLEBOX );

	if(( 1.0f - tr.fraction ) * dist > 8 )
		return -1;
	return dist;
}

static float R_SpriteGlowBlend( ref_entity_t *e )
{
	float	dist = R_GlowSightDistance( e->origin );
	float	brightness;

	if( dist <= 0 ) return 0.0f; // occluded

	if( e->renderfx == kRenderFxNoDissipation )
		return (float)e->renderamt * (1.0f/255.0f);

	// UNDONE: Tweak these magic numbers (19000 - falloff & 200 - sprite size)
	brightness = 19000.0 / ( dist * dist );
	brightness = bound( 0.05f, brightness, 1.0f );

	// Make the glow fixed size in screen space, taking into consideration the scale setting.
	if( e->scale == 0 ) e->scale = 1.0f;
	e->scale *= dist * (1.0f / 200.0f );

	return brightness;
}

qboolean R_SpriteOccluded( ref_entity_t *e )
{
	if( e->rendermode == kRenderGlow )
	{
		float	blend = 1.0f;
		float	depth;
		vec3_t	v;

		R_TransformToScreen_Vec3( e->origin, v );

		if( v[0] < RI.refdef.viewport[0] || v[0] > RI.refdef.viewport[0] + RI.refdef.viewport[2] )
			return true; // do scissor
		if( v[1] < RI.refdef.viewport[1] || v[1] > RI.refdef.viewport[1] + RI.refdef.viewport[3] )
			return true; // do scissor

		if( r_cullflares->integer )
		{
			pglReadPixels((int)( v[0] ), (int)( v[1] ), 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth );
			if( depth + 1e-4 < v[2] ) return true; // occluded
		}

		blend *= R_SpriteGlowBlend( e );
		e->renderamt *= blend;

		if( blend <= 0.05f )
			return true; // faded
	}
	return false;	
}

/*
=================
R_DrawSpriteModel
=================
*/
void R_DrawSpriteModel( const meshbuffer_t *mb )
{
	ref_entity_t	*e;
	mspriteframe_t	*frame, *oldframe;
	msprite_t		*psprite;
	ref_model_t	*model;
	float		lerp = 1.0f, ilerp;
	meshbuffer_t	*rb = (meshbuffer_t *)mb;
	byte		renderamt;

	MB_NUM2ENTITY( mb->sortkey, e );
	model = e->model;

	if( OCCLUSION_QUERIES_ENABLED( RI ) && OCCLUSION_TEST_ENTITY( e ))
	{
		ref_shader_t	*shader;

		MB_NUM2SHADER( mb->shaderkey, shader );
		if( !R_GetOcclusionQueryResultBool( shader->type == 1 ? OQ_PLANARSHADOW : OQ_ENTITY, e - r_entities, true ))
			return;
	}

	psprite = (msprite_t * )model->extradata;

	switch( e->rendermode )
	{
	case kRenderNormal:
	case kRenderTransColor:
	case kRenderTransAlpha:
		frame = oldframe = R_GetSpriteFrame( e->model, e->lerp->curstate.frame, e->angles[YAW] );
		break;
	case kRenderTransTexture:
	case kRenderTransAdd:
	case kRenderGlow:
		lerp = R_GetSpriteFrameInterpolant( e, &oldframe, &frame );
		break;
	default:
		Host_Error( "R_DrawSpriteModel: %s bad rendermode %i\n", model->name, e->rendermode );
		break;
	}

	// do movewith
	if( e->parent && e->movetype == MOVETYPE_FOLLOW )
	{
		if( e->body > 0 && e->parent->model && e->parent->model->type == mod_studio )
		{
			int	num = bound( 1, e->body, MAXSTUDIOATTACHMENTS );

			// pev->colormap is hardcoded to attachment number
			// NOTE: use interpolated origin to avoid flickering attachments
			VectorCopy( e->parent->lerp->attachment[num-1], e->origin2 );
		}
		else VectorCopy( e->parent->origin, e->origin2 );
	}

	// no rotation for sprites
	R_TranslateForEntity( e );

	if( e->rendermode == kRenderGlow )
		pglDepthRange( gldepthmin, gldepthmin + 0.3 * ( gldepthmax - gldepthmin ));
		
	if( oldframe == frame )
	{
		// draw the single non-lerped frame
		if( R_PushSprite( rb, psprite->type, frame->left, frame->right, frame->down, frame->up ))
			R_RenderMeshBuffer( rb );
	}
	else
	{
		// draw two combined lerped frames
		renderamt = e->renderamt;

		lerp = bound( 0.0f, lerp, 1.0f );
		ilerp = 1.0f - lerp;
	
		if( ilerp != 0 )
		{
			e->renderamt = renderamt * ilerp;	// merge prevframe alpha

			if( e->customShader ) rb->shaderkey = e->customShader->sortkey;
			else rb->shaderkey = r_shaders[oldframe->shader].sortkey;
			if( R_PushSprite( rb, psprite->type, oldframe->left, oldframe->right, oldframe->down, oldframe->up ))
				R_RenderMeshBuffer( rb );
		}

		if( lerp != 0 )
		{
			e->renderamt = renderamt * lerp;	// merge frame alpha

			if( e->customShader ) rb->shaderkey = e->customShader->sortkey;
			else rb->shaderkey = r_shaders[frame->shader].sortkey;
			if( R_PushSprite( rb, psprite->type, frame->left, frame->right, frame->down, frame->up ))
				R_RenderMeshBuffer( rb );
		}

		// restore current values (e.g. for right mirror passes)
		e->renderamt = renderamt;
	}

	if( e->rendermode == kRenderGlow )
		pglDepthRange( gldepthmin, gldepthmax );
}

qboolean R_CullSpriteModel( ref_entity_t *e )
{
	qboolean	frustum, query, clipped;

	if( !e->model->extradata )
		return true;

	if( e->ent_type == ET_VIEWENTITY && r_lefthand->integer >= 2 )
		return true;

	VectorCopy( e->model->mins, sprite_mins );
	VectorCopy( e->model->maxs, sprite_maxs );

	sprite_radius = RadiusFromBounds( sprite_mins, sprite_maxs );
	clipped = R_CullModel( e, sprite_mins, sprite_maxs, sprite_radius );
	frustum = clipped & 1;
	if( clipped & 2 ) return true;

	query = OCCLUSION_QUERIES_ENABLED( RI ) && OCCLUSION_TEST_ENTITY( e ) ? true : false;
	if( !frustum && query ) R_IssueOcclusionQuery( R_GetOcclusionQueryNum( OQ_ENTITY, e - r_entities ), e, sprite_mins, sprite_maxs );

	return frustum;
}

void R_SpriteDrawDebug( void )
{
	vec3_t		bbox[8];
	ref_model_t	*model;
	int		i;

	if( r_drawentities->integer != 5 )
		return;

	model = RI.currententity->model;

	// compute a full bounding box
	for( i = 0; i < 8; i++ )
	{
		bbox[i][0] = (i & 1) ? model->mins[0] : model->maxs[0];
		bbox[i][1] = (i & 2) ? model->mins[1] : model->maxs[1];
		bbox[i][2] = (i & 4) ? model->mins[2] : model->maxs[2];
	}

	R_TranslateForEntity( RI.currententity );
	pglColor4f( 0.0f, 0.0f, 1.0f, 1.0f );	// blue bboxes for sprites

	pglBegin( GL_LINES );
	for( i = 0; i < 2; i += 1 )
	{
		pglVertex3fv( bbox[i+0] );
		pglVertex3fv( bbox[i+2] );
		pglVertex3fv( bbox[i+4] );
		pglVertex3fv( bbox[i+6] );
		pglVertex3fv( bbox[i+0] );
		pglVertex3fv( bbox[i+4] );
		pglVertex3fv( bbox[i+2] );
		pglVertex3fv( bbox[i+6] );
		pglVertex3fv( bbox[i*2+0] );
		pglVertex3fv( bbox[i*2+1] );
		pglVertex3fv( bbox[i*2+4] );
		pglVertex3fv( bbox[i*2+5] );
	}
	pglEnd();
}