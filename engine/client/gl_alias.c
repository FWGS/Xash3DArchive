/*
gl_alias.c - alias model renderer
Copyright (C) 2017 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "common.h"
#include "client.h"
#include "mathlib.h"
#include "const.h"
#include "r_studioint.h"
#include "triangleapi.h"
#include "alias.h"
#include "pm_local.h"
#include "gl_local.h"
#include "cl_tent.h"

extern cvar_t r_shadows;

#define NUMVERTEXNORMALS	162
#define SHADEDOT_QUANT	16

float r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

// precalculated dot products for quantized angles
float r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "anorm_dots.h"
;

typedef struct
{
	double		time;
	double		frametime;
	int		framecount;		// studio framecount
} alias_draw_state_t;

static alias_draw_state_t	g_alias;		// global alias state

/*
=================================================================

ALIAS MODEL DISPLAY LIST GENERATION

=================================================================
*/
static model_t	*aliasmodel;
static aliashdr_t	*paliashdr;
static aliashdr_t	*pheader;

static mtrivertex_t	*g_poseverts[MAXALIASFRAMES];
static mtriangle_t	g_triangles[MAXALIASTRIS];
static stvert_t	g_stverts[MAXALIASVERTS];
static qboolean	g_used[8192];

// a pose is a single set of vertexes. a frame may be
// an animating sequence of poses
int		g_posenum;

// the command list holds counts and s/t values that are valid for
// every frame
static int	g_commands[8192];
static int	g_numcommands;

// all frames will have their vertexes rearranged and expanded
// so they are in the order expected by the command list
static int	g_vertexorder[8192];
static int	g_numorder;

static int	g_allverts, g_alltris;

static int	g_stripverts[128];
static int	g_striptris[128];
static int	g_stripcount;

/*
================
StripLength
================
*/
static int StripLength( int starttri, int startv )
{
	int		m1, m2, j, k;
	mtriangle_t	*last, *check;

	g_used[starttri] = 2;

	last = &g_triangles[starttri];

	g_stripverts[0] = last->vertindex[(startv+0) % 3];
	g_stripverts[1] = last->vertindex[(startv+1) % 3];
	g_stripverts[2] = last->vertindex[(startv+2) % 3];

	g_striptris[0] = starttri;
	g_stripcount = 1;

	m1 = last->vertindex[(startv+2)%3];
	m2 = last->vertindex[(startv+1)%3];

nexttri:
	// look for a matching triangle
	for( j = starttri + 1, check = &g_triangles[starttri + 1]; j < pheader->numtris; j++, check++ )
	{
		if( check->facesfront != last->facesfront )
			continue;

		for( k = 0; k < 3; k++ )
		{
			if( check->vertindex[k] != m1 )
				continue;
			if( check->vertindex[(k+1) % 3] != m2 )
				continue;

			// this is the next part of the fan

			// if we can't use this triangle, this tristrip is done
			if( g_used[j] ) goto done;

			// the new edge
			if( g_stripcount & 1 )
				m2 = check->vertindex[(k+2) % 3];
			else m1 = check->vertindex[(k+2) % 3];

			g_stripverts[g_stripcount+2] = check->vertindex[(k+2) % 3];
			g_striptris[g_stripcount] = j;
			g_stripcount++;

			g_used[j] = 2;
			goto nexttri;
		}
	}
done:
	// clear the temp used flags
	for( j = starttri + 1; j < pheader->numtris; j++ )
	{
		if( g_used[j] == 2 )
			g_used[j] = 0;
	}

	return g_stripcount;
}

/*
===========
FanLength
===========
*/
static int FanLength( int starttri, int startv )
{
	int		m1, m2, j, k;
	mtriangle_t	*last, *check;

	g_used[starttri] = 2;

	last = &g_triangles[starttri];

	g_stripverts[0] = last->vertindex[(startv+0) % 3];
	g_stripverts[1] = last->vertindex[(startv+1) % 3];
	g_stripverts[2] = last->vertindex[(startv+2) % 3];

	g_striptris[0] = starttri;
	g_stripcount = 1;

	m1 = last->vertindex[(startv+0) % 3];
	m2 = last->vertindex[(startv+2) % 3];

nexttri:
	// look for a matching triangle
	for( j = starttri + 1, check = &g_triangles[starttri + 1]; j < pheader->numtris; j++, check++ )
	{
		if( check->facesfront != last->facesfront )
			continue;

		for( k = 0; k < 3; k++ )
		{
			if( check->vertindex[k] != m1 )
				continue;
			if( check->vertindex[(k+1) % 3] != m2 )
				continue;

			// this is the next part of the fan
			// if we can't use this triangle, this tristrip is done
			if( g_used[j] ) goto done;

			// the new edge
			m2 = check->vertindex[(k+2) % 3];

			g_stripverts[g_stripcount + 2] = m2;
			g_striptris[g_stripcount] = j;
			g_stripcount++;

			g_used[j] = 2;
			goto nexttri;
		}
	}
done:
	// clear the temp used flags
	for( j = starttri + 1; j < pheader->numtris; j++ )
	{
		if( g_used[j] == 2 )
			g_used[j] = 0;
	}

	return g_stripcount;
}

/*
================
BuildTris

Generate a list of trifans or strips
for the model, which holds for all frames
================
*/
void BuildTris( void )
{
	int	len, bestlen, besttype;
	int	bestverts[1024];
	int	besttris[1024];
	int	type, startv;
	int	i, j, k;
	float	s, t;

	//
	// build tristrips
	//
	memset( g_used, 0, sizeof( g_used ));
	g_numcommands = 0;
	g_numorder = 0;

	for( i = 0; i < pheader->numtris; i++ )
	{
		// pick an unused triangle and start the trifan
		if( g_used[i] ) continue;

		bestlen = 0;
		for( type = 0; type < 2; type++ )
		{
			for( startv = 0; startv < 3; startv++ )
			{
				if( type == 1 ) len = StripLength( i, startv );
				else len = FanLength( i, startv );

				if( len > bestlen )
				{
					besttype = type;
					bestlen = len;

					for( j = 0; j < bestlen + 2; j++ )
						bestverts[j] = g_stripverts[j];

					for( j = 0; j < bestlen; j++ )
						besttris[j] = g_striptris[j];
				}
			}
		}

		// mark the tris on the best strip as used
		for( j = 0; j < bestlen; j++ )
			g_used[besttris[j]] = 1;

		if( besttype == 1 )
			g_commands[g_numcommands++] = (bestlen + 2);
		else g_commands[g_numcommands++] = -(bestlen + 2);

		for( j = 0; j < bestlen + 2; j++ )
		{
			// emit a vertex into the reorder buffer
			k = bestverts[j];
			g_vertexorder[g_numorder++] = k;

			// emit s/t coords into the commands stream
			s = g_stverts[k].s;
			t = g_stverts[k].t;

			if( !g_triangles[besttris[0]].facesfront && g_stverts[k].onseam )
				s += pheader->skinwidth / 2;	// on back side
			s = (s + 0.5f) / pheader->skinwidth;
			t = (t + 0.5f) / pheader->skinheight;

			// Carmack use floats and Valve use shorts here...
			*(float *)&g_commands[g_numcommands++] = s;
			*(float *)&g_commands[g_numcommands++] = t;
		}
	}

	g_commands[g_numcommands++] = 0; // end of list marker

	MsgDev( D_REPORT, "%3i tri %3i vert %3i cmd\n", pheader->numtris, g_numorder, g_numcommands );

	g_alltris += pheader->numtris;
	g_allverts += g_numorder;
}

/*
================
GL_MakeAliasModelDisplayLists
================
*/
void GL_MakeAliasModelDisplayLists( model_t *m, aliashdr_t *hdr )
{
	mtrivertex_t	*verts;
	int		i, j;

	aliasmodel = m;
	paliashdr = hdr;	// (aliashdr_t *)Mod_Extradata (m);

	BuildTris( );

	// save the data out
	paliashdr->poseverts = g_numorder;

	paliashdr->commands = Mem_Alloc( m->mempool, g_numcommands * 4 );
	memcpy( paliashdr->commands, g_commands, g_numcommands * 4 );

	paliashdr->posedata = verts = Mem_Alloc( m->mempool, paliashdr->numposes * paliashdr->poseverts * sizeof( mtrivertex_t ));

	for( i = 0; i < paliashdr->numposes; i++ )
	{
		for( j = 0; j < g_numorder; j++ )
			*verts++ = g_poseverts[i][g_vertexorder[j]];
	}
}

/*
==============================================================================

ALIAS MODELS

==============================================================================
*/
/*
=================
Mod_LoadAliasFrame
=================
*/
void *Mod_LoadAliasFrame( void *pin, maliasframedesc_t *frame )
{
	dtrivertex_t	*pinframe;
	daliasframe_t	*pdaliasframe;
	int		i;

	pdaliasframe = (daliasframe_t *)pin;

	Q_strncpy( frame->name, pdaliasframe->name, sizeof( frame->name ));
	frame->firstpose = g_posenum;
	frame->numposes = 1;

	for( i = 0; i < 3; i++ )
	{
		frame->bboxmin.v[i] = pdaliasframe->bboxmin.v[i];
		frame->bboxmax.v[i] = pdaliasframe->bboxmax.v[i];
	}

	pinframe = (dtrivertex_t *)(pdaliasframe + 1);

	g_poseverts[g_posenum] = (mtrivertex_t *)pinframe;
	g_posenum++;

	pinframe += pheader->numverts;

	return (void *)pinframe;
}

/*
=================
Mod_LoadAliasGroup
=================
*/
void *Mod_LoadAliasGroup( void *pin, maliasframedesc_t *frame )
{
	daliasgroup_t	*pingroup;
	int		i, numframes;
	daliasinterval_t	*pin_intervals;
	void		*ptemp;

	pingroup = (daliasgroup_t *)pin;
	numframes = pingroup->numframes;

	frame->firstpose = g_posenum;
	frame->numposes = numframes;

	for( i = 0; i < 3; i++ )
	{
		frame->bboxmin.v[i] = pingroup->bboxmin.v[i];
		frame->bboxmax.v[i] = pingroup->bboxmax.v[i];
	}

	pin_intervals = (daliasinterval_t *)(pingroup + 1);

	frame->interval = pin_intervals->interval;
	pin_intervals += numframes;
	ptemp = (void *)pin_intervals;

	for( i = 0; i < numframes; i++ )
	{
		g_poseverts[g_posenum] = (mtrivertex_t *)((daliasframe_t *)ptemp + 1);
		g_posenum++;

		ptemp = ((daliasframe_t *)ptemp + 1) + pheader->numverts;
	}

	return ptemp;
}

/*
=================================================================

SKIN REFLOODING

=================================================================
*/
typedef struct
{
	short	x, y;
} floodfill_t;

// must be a power of 2
#define FLOODFILL_FIFO_SIZE	0x1000
#define FLOODFILL_FIFO_MASK	(FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP( off, dx, dy ) \
{ \
	if( pos[off] == fillcolor ) \
	{ \
		pos[off] = 255; \
		fifo[inpt].x = x + (dx), fifo[inpt].y = y + (dy); \
		inpt = (inpt + 1) & FLOODFILL_FIFO_MASK; \
	} \
	else if( pos[off] != 255 ) fdc = pos[off]; \
}

/*
=================
Mod_FloodFillSkin

Fill background pixels so mipmapping doesn't have haloes - Ed
=================
*/
void Mod_FloodFillSkin( byte *skin, int skinwidth, int skinheight )
{
	byte		fillcolor = *skin; // assume this is the pixel to fill
	floodfill_t	fifo[FLOODFILL_FIFO_SIZE];
	int		inpt = 0, outpt = 0;
	int		filledcolor = 0; // g-cont. opaque black is probably always 0-th index

	// can't fill to filled color or to transparent color (used as visited marker)
	if(( fillcolor == filledcolor ) || ( fillcolor == 255 ))
		return;

	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	while( outpt != inpt )
	{
		int	x = fifo[outpt].x, y = fifo[outpt].y;
		byte	*pos = &skin[x + skinwidth * y];
		int	fdc = filledcolor;

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if( x > 0 ) FLOODFILL_STEP( -1, -1, 0 );
		if( x < skinwidth - 1 ) FLOODFILL_STEP( 1, 1, 0 );
		if( y > 0 ) FLOODFILL_STEP( -skinwidth, 0, -1 );
		if( y < skinheight - 1 ) FLOODFILL_STEP( skinwidth, 0, 1 );
		skin[x + skinwidth * y] = fdc;
	}
}

/*
===============
Mod_CreateSkinData
===============
*/
rgbdata_t *Mod_CreateSkinData( byte *data, int width, int height )
{
	static rgbdata_t	skin;

	skin.width = width;
	skin.height = height;
	skin.depth = 1;
	skin.type = PF_INDEXED_24;
	SetBits( skin.flags, IMAGE_HAS_COLOR );
	skin.encode = DXT_ENCODE_DEFAULT;
	skin.numMips = 1;
	skin.buffer = data;
	skin.palette = (byte *)&clgame.palette;
	skin.size = width * height;

	// make an copy
	return FS_CopyImage( &skin );
}

/*
===============
Mod_LoadAllSkins
===============
*/
void *Mod_LoadAllSkins( int numskins, daliasskintype_t *pskintype )
{
	char			name[32];
	daliasskingroup_t		*pinskingroup;
	daliasskininterval_t	*pinskinintervals;
	int			size, groupskins;
	int			i, j, k;
	byte			*skin;
	rgbdata_t			*pic;

	skin = (byte *)(pskintype + 1);

	if( numskins < 1 || numskins > MAX_SKINS )
		Host_Error( "Mod_LoadAliasModel: Invalid # of skins: %d\n", numskins );

	size = pheader->skinwidth * pheader->skinheight;

	for( i = 0; i < numskins; i++ )
	{
		if( pskintype->type == ALIAS_SKIN_SINGLE )
		{
			Mod_FloodFillSkin( skin, pheader->skinwidth, pheader->skinheight );

			// save 8 bit texels for the player model to remap
			pheader->texels[i] = Mem_Alloc( loadmodel->mempool, size );
			memcpy( pheader->texels[i], (byte *)(pskintype + 1), size );

			Q_snprintf( name, sizeof( name ), "%s:frame%i", loadmodel->name, i );
			pic = Mod_CreateSkinData( (byte *)(pskintype + 1), pheader->skinwidth, pheader->skinheight );

			pheader->gl_texturenum[i][0] =
			pheader->gl_texturenum[i][1] =
			pheader->gl_texturenum[i][2] =
			pheader->gl_texturenum[i][3] = GL_LoadTextureInternal( name, pic, 0, false );
			FS_FreeImage( pic );
			pskintype = (daliasskintype_t *)((byte *)(pskintype + 1) + size);
		}
		else
		{
			// animating skin group.  yuck.
			pskintype++;
			pinskingroup = (daliasskingroup_t *)pskintype;
			groupskins = pinskingroup->numskins;
			pinskinintervals = (daliasskininterval_t *)(pinskingroup + 1);
			pskintype = (void *)(pinskinintervals + groupskins);

			for( j = 0; j < groupskins; j++ )
			{
				Mod_FloodFillSkin( skin, pheader->skinwidth, pheader->skinheight );
				if( j == 0 )
				{
					pheader->texels[i] = Mem_Alloc( loadmodel->mempool, size );
					memcpy( pheader->texels[i], (byte *)(pskintype), size );
				}
				Q_snprintf( name, sizeof( name ), "%s_%i_%i", loadmodel->name, i, j );
				pic = Mod_CreateSkinData( (byte *)(pskintype), pheader->skinwidth, pheader->skinheight );
				pheader->gl_texturenum[i][j & 3] = GL_LoadTextureInternal( name, pic, 0, false );
				pskintype = (daliasskintype_t *)((byte *)(pskintype) + size);
				FS_FreeImage( pic );
			}

			for( k = j; j < 4; j++ )
				pheader->gl_texturenum[i][j & 3] = pheader->gl_texturenum[i][j - k]; 
		}
	}

	return (void *)pskintype;
}

//=========================================================================
/*
=================
Mod_CalcAliasBounds
=================
*/
void Mod_CalcAliasBounds( aliashdr_t *a )
{
	int	i, j, k;
	float	radius;
	float	dist;
	vec3_t	v;

	ClearBounds( loadmodel->mins, loadmodel->maxs );
	radius = 0.0f;

	// process verts
	for( i = 0; i < a->numposes; i++ )
	{
		for( j = 0; j < a->numverts; j++ )
		{
			for( k = 0; k < 3; k++ )
				v[k] = g_poseverts[i][j].v[k] * pheader->scale[k] + pheader->scale_origin[k];

			AddPointToBounds( v, loadmodel->mins, loadmodel->maxs );
			dist = DotProduct( v, v );

			if( radius < dist )
				radius = dist;
		}
	}

	loadmodel->radius = sqrt( radius );
}

/*
=================
Mod_LoadAliasModel
=================
*/
void Mod_LoadAliasModel( model_t *mod, const void *buffer, qboolean *loaded )
{
	daliashdr_t	*pinmodel;
	stvert_t		*pinstverts;
	dtriangle_t	*pintriangles;
	int		numframes, size;
	daliasframetype_t	*pframetype;
	daliasskintype_t	*pskintype;
	int		i, j;

	if( loaded ) *loaded = false;
	pinmodel = (daliashdr_t *)buffer;
	i = pinmodel->version;

	if( i != ALIAS_VERSION )
	{
		MsgDev( D_ERROR, "%s has wrong version number (%i should be %i)\n", mod->name, i, ALIAS_VERSION );
		return;
	}

	mod->mempool = Mem_AllocPool( va( "^2%s^7", mod->name ));

	// allocate space for a working header, plus all the data except the frames,
	// skin and group info
	size = sizeof( aliashdr_t ) + (pinmodel->numframes - 1) * sizeof( pheader->frames[0] );

	pheader = Mem_Alloc( mod->mempool, size );
	mod->flags = pinmodel->flags;	// share effects flags

	// endian-adjust and copy the data, starting with the alias model header
	pheader->boundingradius = pinmodel->boundingradius;
	pheader->numskins = pinmodel->numskins;
	pheader->skinwidth = pinmodel->skinwidth;
	pheader->skinheight = pinmodel->skinheight;
	pheader->numverts = pinmodel->numverts;

	if( pheader->numverts <= 0 )
	{
		MsgDev( D_ERROR, "model %s has no vertices\n", mod->name );
		return;
	}

	if( pheader->numverts > MAXALIASVERTS )
	{
		MsgDev( D_ERROR, "model %s has too many vertices\n", mod->name );
		return;
	}

	pheader->numtris = pinmodel->numtris;

	if( pheader->numtris <= 0 )
	{
		MsgDev( D_ERROR, "model %s has no triangles\n", mod->name );
		return;
	}

	pheader->numframes = pinmodel->numframes;
	numframes = pheader->numframes;

	if( numframes < 1 )
	{
		MsgDev( D_ERROR, "Mod_LoadAliasModel: Invalid # of frames: %d\n", numframes );
		return;
	}

	pheader->size = pinmodel->size;
//	mod->synctype = pinmodel->synctype;
	mod->numframes = pheader->numframes;

	for( i = 0; i < 3; i++ )
	{
		pheader->scale[i] = pinmodel->scale[i];
		pheader->scale_origin[i] = pinmodel->scale_origin[i];
		pheader->eyeposition[i] = pinmodel->eyeposition[i];
	}

	// load the skins
	pskintype = (daliasskintype_t *)&pinmodel[1];
	pskintype = Mod_LoadAllSkins( pheader->numskins, pskintype );

	// load base s and t vertices
	pinstverts = (stvert_t *)pskintype;

	for( i = 0; i < pheader->numverts; i++ )
	{
		g_stverts[i].onseam = pinstverts[i].onseam;
		g_stverts[i].s = pinstverts[i].s;
		g_stverts[i].t = pinstverts[i].t;
	}

	// load triangle lists
	pintriangles = (dtriangle_t *)&pinstverts[pheader->numverts];

	for( i = 0; i < pheader->numtris; i++ )
	{
		g_triangles[i].facesfront = pintriangles[i].facesfront;

		for( j = 0; j < 3; j++ )
			g_triangles[i].vertindex[j] = pintriangles[i].vertindex[j];
	}

	// load the frames
	pframetype = (daliasframetype_t *)&pintriangles[pheader->numtris];
	g_posenum = 0;

	for( i = 0; i < numframes; i++ )
	{
		aliasframetype_t	frametype = pframetype->type;

		if( frametype == ALIAS_SINGLE )
			pframetype = (daliasframetype_t *)Mod_LoadAliasFrame( pframetype + 1, &pheader->frames[i] );
		else pframetype = (daliasframetype_t *)Mod_LoadAliasGroup( pframetype + 1, &pheader->frames[i] );
	}

	pheader->numposes = g_posenum;
	Mod_CalcAliasBounds( pheader );
	mod->type = mod_alias;

	// build the draw lists
	GL_MakeAliasModelDisplayLists( mod, pheader );

	// move the complete, relocatable alias model to the cache
	loadmodel->cache.data = pheader;

	if( loaded ) *loaded = true;	// done
}

/*
=================
Mod_UnloadAliasModel
=================
*/
void Mod_UnloadAliasModel( model_t *mod )
{
	aliashdr_t	*palias;
	int		i;

	ASSERT( mod != NULL );

	if( mod->type != mod_alias )
		return; // not an alias

	palias = mod->cache.data;
	if( !palias ) return; // already freed

	// FIXME: check animating groups too?
	for( i = 0; i < MAX_SKINS; i++ )
	{
		if( !palias->gl_texturenum[i][0] )
			continue;
		GL_FreeTexture( palias->gl_texturenum[i][0] );
	}

	Mem_FreePool( &mod->mempool );
	memset( mod, 0, sizeof( *mod ));
}

/*
=============================================================

  ALIAS MODELS

=============================================================
*/
vec3_t	shadevector;
float	shadelight, ambientlight;
float	*shadedots = r_avertexnormal_dots[0];
colorVec	lightColor;
vec3_t	lightspot;
int	lastposenum;

/*
=============
GL_DrawAliasFrame
=============
*/
void GL_DrawAliasFrame( aliashdr_t *paliashdr, int posenum )
{
	float 		l;
	mtrivertex_t	*verts;
	int		*order;
	int		count;

lastposenum = posenum;

	verts = paliashdr->posedata;
	verts += posenum * paliashdr->poseverts;
	order = paliashdr->commands;

	while( 1 )
	{
		// get the vertex count and primitive type
		count = *order++;
		if( !count ) break; // done

		if( count < 0 )
		{
			pglBegin( GL_TRIANGLE_FAN );
			count = -count;
		}
		else
		{
			pglBegin( GL_TRIANGLE_STRIP );
                    }

		do
		{
			// texture coordinates come from the draw list
			pglTexCoord2f (((float *)order)[0], ((float *)order)[1]);
			order += 2;

			// normals and vertexes come from the frame list
			l = shadedots[verts->lightnormalindex] * shadelight;
			pglColor3f( l, l, l );
			pglVertex3f( verts->v[0], verts->v[1], verts->v[2] );
			verts++;
		} while( --count );

		pglEnd();
	}
}

/*
=============
GL_DrawAliasShadow
=============
*/
void GL_DrawAliasShadow( aliashdr_t *paliashdr, int posenum )
{
	mtrivertex_t	*verts;
	int		*order;
	vec3_t		point;
	float		height, lheight;
	int		count;

	lheight = RI.currententity->origin[2] - lightspot[2];

	height = 0;
	verts = paliashdr->posedata;
	verts += posenum * paliashdr->poseverts;
	order = paliashdr->commands;

	height = -lheight + 1.0;

	while( 1 )
	{
		// get the vertex count and primitive type
		count = *order++;
		if( !count ) break; // done

		if( count < 0 )
		{
			pglBegin( GL_TRIANGLE_FAN );
			count = -count;
		}
		else
		{
			pglBegin( GL_TRIANGLE_STRIP );
		}

		do
		{
			// texture coordinates come from the draw list
			// (skipped for shadows) pglTexCoord2fv ((float *)order);
			order += 2;

			// normals and vertexes come from the frame list
			point[0] = verts->v[0] * paliashdr->scale[0] + paliashdr->scale_origin[0];
			point[1] = verts->v[1] * paliashdr->scale[1] + paliashdr->scale_origin[1];
			point[2] = verts->v[2] * paliashdr->scale[2] + paliashdr->scale_origin[2];

			point[0] -= shadevector[0] * (point[2] + lheight);
			point[1] -= shadevector[1] * (point[2] + lheight);
			point[2] = height;
//			height -= 0.001f;
			pglVertex3fv( point );

			verts++;
		} while( --count );

		pglEnd();
	}	
}

/*
=================
R_SetupAliasFrame

=================
*/
void R_SetupAliasFrame( int frame, aliashdr_t *paliashdr )
{
	int	pose, numposes;
	float	interval;

	if(( frame >= paliashdr->numframes ) || ( frame < 0 ))
	{
		MsgDev( D_ERROR, "R_AliasSetupFrame: no such frame %d\n", frame );
		frame = 0;
	}

	pose = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	if( numposes > 1 )
	{
		interval = paliashdr->frames[frame].interval;
		pose += (int)(cl.time / interval) % numposes;
	}

	GL_DrawAliasFrame( paliashdr, pose );
}

/*
=================
R_DrawAliasModel

=================
*/
void R_DrawAliasModel( cl_entity_t *e )
{
	int			i;
	int			lnum;
	vec3_t		dist, p1;
	float		add;
	model_t		*clmodel;
	vec3_t		absmin, absmax;
	aliashdr_t	*paliashdr;
	float		an;
	int			anim;

	clmodel = RI.currententity->model;

	VectorAdd( RI.currententity->origin, clmodel->mins, absmin );
	VectorAdd( RI.currententity->origin, clmodel->maxs, absmax );

	if( R_CullModel( e, absmin, absmax ))
		return;

	//
	// get lighting information
	//
	VectorSet( p1, RI.currententity->origin[0], RI.currententity->origin[1], RI.currententity->origin[2] - 2048.0f );
	lightColor = R_LightVec( RI.currententity->origin, p1, lightspot );

	ambientlight = shadelight = 0;	// FIXME: compute light

	// allways give the gun some light
	if (e == &clgame.viewent && ambientlight < 24)
		ambientlight = shadelight = 24;

	for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
	{
		if (cl_dlights[lnum].die >= cl.time)
		{
			VectorSubtract (e->origin,
							cl_dlights[lnum].origin,
							dist);
			add = cl_dlights[lnum].radius - VectorLength(dist);

			if (add > 0) {
				ambientlight += add;
				//ZOID models should be affected by dlights as well
				shadelight += add;
			}
		}
	}

	// clamp lighting so it doesn't overbright as much
	if (ambientlight > 128)
		ambientlight = 128;
	if (ambientlight + shadelight > 192)
		shadelight = 192 - ambientlight;

	// ZOID: never allow players to go totally black
	i = e - clgame.entities;
	if (i >= 1 && i<=cl.maxclients /* && !strcmp (currententity->model->name, "progs/player.mdl") */)
		if (ambientlight < 8)
			ambientlight = shadelight = 8;

	if( FBitSet( e->curstate.effects, EF_FULLBRIGHT ))
		ambientlight = shadelight = 256;

	shadedots = r_avertexnormal_dots[((int)(e->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];
	shadelight = shadelight / 200.0;
	
	an = e->angles[1]/180*M_PI;
	shadevector[0] = cos(-an);
	shadevector[1] = sin(-an);
	shadevector[2] = 1;
	VectorNormalize (shadevector);

	//
	// locate the proper data
	//
	paliashdr = (aliashdr_t *)Mod_Extradata( RI.currententity->model );

	r_stats.c_alias_polys += paliashdr->numtris;

	//
	// draw all the triangles
	//

	R_RotateForEntity( e );

	pglTranslatef( paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2] );
	pglScalef( paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2] );

	anim = (int)(cl.time*10) & 3;
	GL_Bind( GL_TEXTURE0, paliashdr->gl_texturenum[RI.currententity->curstate.skin][anim]);
#if 0
	// we can't dynamically colormap textures, so they are cached
	// seperately for the players.  Heads are just uncolored.
	if (RI.currententity->colormap != vid.colormap && !gl_nocolors.value)
	{
		i = currententity - cl_entities;
		if (i >= 1 && i<=cl.maxclients /* && !strcmp (currententity->model->name, "progs/player.mdl") */)
		    GL_Bind(playertextures - 1 + i);
	}
#endif
	pglShadeModel( GL_SMOOTH );
	pglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	R_SetupAliasFrame( RI.currententity->curstate.frame, paliashdr );

	pglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );

	pglShadeModel( GL_FLAT );
	R_LoadIdentity();

	if( r_shadows.value )
	{
		pglPushMatrix ();
		R_RotateForEntity (e);
		pglDisable (GL_TEXTURE_2D);
		pglEnable (GL_BLEND);
		pglColor4f (0,0,0,0.5);
		GL_DrawAliasShadow (paliashdr, lastposenum);
		pglEnable (GL_TEXTURE_2D);
		pglDisable (GL_BLEND);
		pglColor4f (1,1,1,1);
		pglPopMatrix ();
	}

}

//==================================================================================