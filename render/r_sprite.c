//=======================================================================
//			Copyright XashXT Group 2007 �
//		        r_sprite.c - render sprite models
//=======================================================================

#include "gl_local.h"

/*
=============================================================

  SPRITE MODELS

=============================================================
*/

/*
====================
Sprite model loader
====================
*/
void *R_SpriteLoadFrame (model_t *mod, void *pin, mspriteframe_t **ppframe, int framenum, byte *pal )
{
	dspriteframe_t	*pinframe;
	mspriteframe_t	*pspriteframe;
	int		width, height, size, origin[2];
	char		name[64];
          image_t		*image;
	
	pinframe = (dspriteframe_t *)pin;

	width = LittleLong (pinframe->width);
	height = LittleLong (pinframe->height);
	size = width * height;
          
	pspriteframe = Mem_Alloc(mod->mempool, sizeof (mspriteframe_t));
	memset (pspriteframe, 0, sizeof (mspriteframe_t));

	*ppframe = pspriteframe;

	pspriteframe->width = width;
	pspriteframe->height = height;
	origin[0] = LittleLong (pinframe->origin[0]);
	origin[1] = LittleLong (pinframe->origin[1]);

	pspriteframe->up = origin[1];
	pspriteframe->down = origin[1] - height;
	pspriteframe->left = origin[0];
	pspriteframe->right = width + origin[0];
          pspriteframe->texnum = 0;
	
	//extract sprite name from path
	ri.FS_FileBase( mod->name, name );
	strcat(name, va("_%i", framenum));

	image = R_LoadImage( name, (byte *)(pinframe + 1), pal, width, height, it_sprite, 32, true );
	
	if(image)
	{
		pspriteframe->texnum = image->texnum;
		mod->skins[framenum] = image;
          }
          else Msg("Warning: %s has null frame %d\n", image->name, framenum );
          
	return (void *)((byte *)(pinframe+1) + size);
}

void *R_SpriteLoadGroup (model_t *mod, void * pin, mspriteframe_t **ppframe, int framenum, byte *pal )
{
	dspritegroup_t	*pingroup;
	mspritegroup_t	*pspritegroup;
	int		i, numframes;
	dspriteinterval_t	*pin_intervals;
	float		*poutintervals;
	void		*ptemp;

	pingroup = (dspritegroup_t *)pin;
	numframes = LittleLong (pingroup->numframes);

	pspritegroup = Mem_Alloc(mod->mempool, sizeof (mspritegroup_t) + (numframes - 1) * sizeof (pspritegroup->frames[0]));
	pspritegroup->numframes = numframes;

	*ppframe = (mspriteframe_t *)pspritegroup;
	pin_intervals = (dspriteinterval_t *)(pingroup + 1);
	poutintervals = Mem_Alloc(mod->mempool, numframes * sizeof (float));

	pspritegroup->intervals = poutintervals;

	for (i = 0; i < numframes; i++)
	{
		*poutintervals = LittleFloat (pin_intervals->interval);
		if (*poutintervals <= 0.0) 
		{
			*poutintervals = 0.1f;//may be out of range. use with caution
			Msg("Warning: %s with interval <= 0.0f, was merged\n", mod->name );
		}
		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void *)pin_intervals;

	for (i = 0; i < numframes; i++ )
	{
		ptemp = R_SpriteLoadFrame(mod, ptemp, &pspritegroup->frames[i], framenum + i, pal);
	}
	return ptemp;
}

void R_SpriteLoadModel( model_t *mod, void *buffer )
{
	int		i, size, version, numframes;
	dsprite_t		*pin;
	short		*numi;
	msprite_t		*psprite;
	dspriteframetype_t	*pframetype;
	byte		pal[256][4];
	vec4_t		rgbacolor;
	float		framerate;
	
	pin = (dsprite_t *)buffer;
	version = LittleLong (pin->version);
		
	switch( version )
	{
	case SPRITE_VERSION_HALF:
		ResetRGBA( rgbacolor );
		framerate = 0.0f;
		break;
	case SPRITE_VERSION_XASH:
		//unpack rgba
		rgbacolor[0] = LittleLong((pin->rgbacolor & 0x000000FF) >> 0);
		rgbacolor[1] = LittleLong((pin->rgbacolor & 0x0000FF00) >> 8);
		rgbacolor[2] = LittleLong((pin->rgbacolor & 0x00FF0000) >> 16);
		rgbacolor[3] = LittleLong((pin->rgbacolor & 0xFF000000) >> 24);

		TransformRGBA(rgbacolor, rgbacolor );//convert into float
		framerate = LittleFloat (pin->framerate);
		break;
	default:
		Msg("Warning: %s has wrong version number (%i should be %i or %i)", mod->name, version, SPRITE_VERSION_HALF, SPRITE_VERSION_XASH );
		return;
	}

	numframes = LittleLong (pin->numframes);
	size = sizeof (msprite_t) + (numframes - 1) * sizeof (psprite->frames);

	psprite = Mem_Alloc(mod->mempool, size );
	mod->extradata = psprite; //make link to extradata
	
	psprite->type	= LittleLong(pin->type);
	psprite->maxwidth	= LittleLong (pin->width);
	psprite->maxheight	= LittleLong (pin->height);
	psprite->rendermode = LittleLong (pin->texFormat);
	psprite->framerate	= framerate;
	psprite->numframes	= numframes;
          memcpy(psprite->rgba, rgbacolor, sizeof(psprite->rgba));
	numi = (short *)(pin + 1);
	
	mod->mins[0] = mod->mins[1] = -psprite->maxwidth / 2;
	mod->maxs[0] = mod->maxs[1] = psprite->maxwidth / 2;
	mod->mins[2] = -psprite->maxheight / 2;
	mod->maxs[2] = psprite->maxheight / 2;
	
	if (LittleShort(*numi) == 256)
	{	
		byte *src = (byte *)(numi+1);

		switch( psprite->rendermode )
		{
		case SPR_NORMAL:
			for (i = 0; i < 256; i++)
			{
				pal[i][0] = *src++;
				pal[i][1] = *src++;
				pal[i][2] = *src++;
				pal[i][3] = 0;
			}
			break;
		case SPR_ADDGLOW:
		case SPR_ADDITIVE:
			for (i = 0; i < 256; i++)
			{
				pal[i][0] = *src++;
				pal[i][1] = *src++;
				pal[i][2] = *src++;
				pal[i][3] = 255;
			}
			break;
                    case SPR_INDEXALPHA:
			for (i = 0; i < 256; i++)
			{
				pal[i][0] = *src++;
				pal[i][1] = *src++;
				pal[i][2] = *src++;
				pal[i][3] = i;
			}
			break;
		case SPR_ALPHTEST:		
			for (i = 0; i < 256; i++)
			{
				pal[i][0] = *src++;
				pal[i][1] = *src++;
				pal[i][2] = *src++;
				pal[i][3] = 255;
			}
			pal[255][0] = pal[255][1] = pal[255][2] = pal[255][3] = 0;
                              break;
		default:
			for (i = 0; i < 256; i++)
			{
				pal[i][0] = *src++;
				pal[i][1] = *src++;
				pal[i][2] = *src++;
				pal[i][3] = 0;
			}
			Msg("Warning: %s has unknown texFormat (%i, should be in range 0-4 )\n", mod->name, psprite->rendermode );
			break;
		}
		pframetype = (dspriteframetype_t *)(src);
	}
	else 
	{
		Msg("Warning: %s has wrong number of palette colors %i (should be 256)\n", mod->name, numi);
		return;
	}		
	if(numframes < 1)
	{
		Msg("Warning: %s has invalid # of frames: %d\n", mod->name, numframes );
		return;
	}	

	mod->numframes = mod->numtexinfo = numframes;
	mod->registration_sequence = registration_sequence;
	
	for (i = 0; i < numframes; i++ )
	{
		frametype_t frametype;

		frametype = LittleLong (pframetype->type);
		psprite->frames[i].type = frametype;

		if (frametype == SPR_SINGLE)
		{
			pframetype = (dspriteframetype_t *)R_SpriteLoadFrame(mod, pframetype + 1, &psprite->frames[i].frameptr, i, (byte *)(&pal[0][0]));
		}
		else if(frametype == SPR_GROUP)
		{
			pframetype = (dspriteframetype_t *)R_SpriteLoadGroup(mod, pframetype + 1, &psprite->frames[i].frameptr, i, (byte *)(&pal[0][0]));
		}
		if(pframetype == NULL) break;                                                   
	}
}

/*
================
R_GetSpriteFrame
================
*/
mspriteframe_t *R_GetSpriteFrame (entity_t *currententity)
{
	msprite_t		*psprite;
	mspritegroup_t	*pspritegroup;
	mspriteframe_t	*pspriteframe;
	int		i, numframes, frame;
	float		*pintervals, fullinterval, targettime, time;

	psprite = currententity->model->extradata;
	frame = currententity->frame;

	if ((frame >= psprite->numframes) || (frame < 0))
	{
		Com_Printf ("R_DrawSprite: no such frame %d (%s)\n", frame, currententity->model->name);
		frame = 0;
	}

	if (psprite->frames[frame].type == SPR_SINGLE)
	{
		pspriteframe = psprite->frames[frame].frameptr;
	}
	else
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];

		time = r_newrefdef.time / 1000.0f;

		// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
		// are positive, so we don't have to worry about division by 0
		targettime = time - ((int)(time / fullinterval)) * fullinterval;

		for (i = 0; i<(numframes - 1); i++)
		{
			if (pintervals[i] > targettime)
				break;
		}

		pspriteframe = pspritegroup->frames[i];
	}

	return pspriteframe;
}


void R_SpriteDrawModel( int passnum )
{
	float alpha = 1.0F;
	mspriteframe_t	*frame;
	vec3_t		point, forward, right, up;
	msprite_t		*psprite;
	entity_t 		*e = currententity;
	model_t		*mod = currentmodel;
	float		realtime = r_newrefdef.time / 1000.000f;

	if ( (e->flags & RF_TRANSLUCENT) && (passnum == RENDERPASS_SOLID)) return;// solid
	if (!(e->flags & RF_TRANSLUCENT) && (passnum == RENDERPASS_ALPHA)) return;// solid
	
	// don't even bother culling, because it's just a single
	// polygon without a surface cache

	psprite = (msprite_t *)mod->extradata;

	//xash auto-animate
	if(psprite->framerate > 0 && psprite->numframes > 1)
	{
		float frametime;

		mod->prevanimtime = mod->animtime;
		mod->animtime = realtime;
		frametime = mod->animtime - mod->prevanimtime;
		if(frametime == 0 ) return;//first call
		
		mod->frame += psprite->framerate * frametime;		
		while (mod->frame > psprite->numframes) mod->frame -= psprite->numframes;
		while (mod->frame < 0) mod->frame += psprite->numframes;
		e->frame = mod->frame;//set properly frame
	}
	else e->frame = fmod(e->frame, psprite->numframes);
	frame = R_GetSpriteFrame(e);
	
	switch( psprite->type )
	{
	case SPR_ORIENTED:
		AngleVectors (e->angles, forward, right, up);
		VectorScale(forward, 0.01, forward );//offset for decals
		VectorSubtract(e->origin, forward, e->origin );
		break;
	case SPR_FACING_UPRIGHT:
		up[0] = up[1] = 0;
		up[2] = 1;
		right[0] = e->origin[1] - r_origin[1];
		right[1] = -(e->origin[0] - r_origin[0]);
		right[2] = 0;
		VectorNormalize (right);
		VectorCopy (vforward, forward);
		break;
	case SPR_VP_PARALLEL_UPRIGHT:
		up[0] = up[1] = 0;
		up[2] = 1;
		VectorCopy (vup, right);
		VectorCopy (vforward, forward);
		break;
	default:
		// normal sprite
		VectorCopy (vup, up);
		VectorCopy (vright, right);
		VectorCopy (vforward, forward);
		break;
	}
	
	if ( e->flags & RF_TRANSLUCENT ) alpha = e->alpha;
	else alpha = psprite->rgba[3];

	if ( e->scale == 0 ) e->scale = 1.0f;
	if ( alpha != 1.0f ) qglEnable( GL_BLEND );

	qglColor4f( psprite->rgba[0], psprite->rgba[1], psprite->rgba[2], alpha );
 	if(psprite->rendermode == SPR_ADDGLOW) qglDisable(GL_DEPTH_TEST);
	
	GL_Bind(currentmodel->skins[(int)e->frame]->texnum);
	GL_TexEnv( GL_MODULATE );

	if ( alpha == 1.0 ) qglEnable (GL_ALPHA_TEST);
	else qglDisable( GL_ALPHA_TEST );

	qglDisable(GL_CULL_FACE);
	qglBegin (GL_QUADS);
		qglTexCoord2f (0, 0);
		VectorMA (e->origin, frame->up * e->scale, up, point);
		VectorMA (point, frame->left * e->scale, right, point);
		qglVertex3fv (point);

		qglTexCoord2f (1, 0);
		VectorMA (e->origin, frame->up * e->scale, up, point);
		VectorMA (point, frame->right * e->scale, right, point);
		qglVertex3fv (point);

		qglTexCoord2f (1, 1);
		VectorMA (e->origin, frame->down * e->scale, up, point);
		VectorMA (point, frame->right * e->scale, right, point);
		qglVertex3fv (point);

          	qglTexCoord2f (0, 1);
		VectorMA (e->origin, frame->down * e->scale, up, point);
		VectorMA (point, frame->left * e->scale, right, point);
		qglVertex3fv (point);
	qglEnd ();

	qglDisable(GL_BLEND);
	qglDisable (GL_ALPHA_TEST);
	qglEnable(GL_DEPTH_TEST);
	GL_TexEnv( GL_REPLACE );

	if(psprite->rendermode == SPR_ADDGLOW) qglEnable(GL_DEPTH_TEST);
	if ( alpha != 1.0F ) qglDisable( GL_BLEND );
	qglColor4f( 1, 1, 1, 1 );
}

