//=======================================================================
//			Copyright XashXT Group 2010 ©
//			gl_sprite.c - sprite rendering
//=======================================================================

#include "common.h"
#include "client.h"
#include "gl_local.h"
#include "sprite.h"

#define MAPSPRITE_SIZE	128	// it's a Valve default value for LoadMapSprite (probably must be power of two)

char	sprite_name[64];
char	group_suffix[8];

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

	Msg( "Load: %s\n", texname );

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
		case SPR_ADDGLOW:
			pal = FS_LoadImage( "#normal.pal", src, 768 );
			break;
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

	Msg( "%s %i frames\n", mod->name, numframes );

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