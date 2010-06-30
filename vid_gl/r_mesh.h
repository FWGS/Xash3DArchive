//=======================================================================
//			Copyright XashXT Group 2009 ©
//		        r_mesh.h - generic mesh container
//=======================================================================

#ifndef R_MESH_H
#define R_MESH_H

enum
{
	MF_NONE		= 0,
	MF_NORMALS	= BIT( 0 ),
	MF_STCOORDS	= BIT( 1 ),
	MF_LMCOORDS	= BIT( 2 ),
	MF_COLORS		= BIT( 3 ),
	MF_ENABLENORMALS    = BIT( 4 ),
	MF_DEFORMVS	= BIT( 5 ),
	MF_SVECTORS	= BIT( 6 ),
	MF_TVECTORS	= BIT( 7 ),

	// global features
	MF_NOCULL		= BIT( 16 ),
	MF_TRIFAN		= BIT( 17 ),
	MF_NONBATCHED	= BIT( 18 ),
	MF_KEEPLOCK	= BIT( 19 )
};

enum
{
	MB_MODEL,
	MB_POLY,
	MB_DECAL,
	MB_CORONA,
	MB_MAXTYPES = 4
};

typedef struct mesh_s
{
	int		numVerts;
	int		numElems;

	vec4_t		*vertexArray;
	vec4_t		*normalsArray;
	vec4_t		*sVectorsArray;
	vec4_t		*tVectorsArray;
	vec2_t		*stCoordArray;
	vec2_t		*lmCoordArray;
	rgba_t		*colorsArray;
	elem_t		*elems;

	struct mesh_s	*next;		// temporary chain of subdivided surfaces
} mesh_t;

#define MB_FOG2NUM( fog )			((fog) ? ((((int)((fog) - r_worldbrushmodel->fogs))+1) << 2) : 0 )
#define MB_NUM2FOG( num, fog )		((fog) = r_worldbrushmodel->fogs+(((num)>>2) & 0xFF), (fog) = ( (fog) == r_worldbrushmodel->fogs ? NULL : (fog)-1 ))

#define MB_ENTITY2NUM( ent )			((int)((ent)-r_entities)<<20 )
#define MB_NUM2ENTITY( num, ent )		(ent = r_entities+(((num)>>20)&MAX_ENTITIES-1))

#define MB_SHADER2NUM( s )			((s)->sort << 26 ) | ((s) - r_shaders )
#define MB_NUM2SHADER( num, s )		((s) = r_shaders + ((num) & 0xFFF))
#define MB_NUM2SHADERSORT( num )		(((num) >> 26) & 0x1F )

#define MIN_RENDER_MESHES			2048

typedef struct
{
	int		shaderkey;
	uint		sortkey;
	int		infokey;		// surface number or mesh number
	union
	{
		int	lastPoly;
		uint	dlightbits;
		uint	modhandle;
	};
	uint		shadowbits;
} meshbuffer_t;

typedef struct
{
	int			num_opaque_meshes, max_opaque_meshes;
	meshbuffer_t		*meshbuffer_opaque;

	int			num_translucent_meshes, max_translucent_meshes;
	meshbuffer_t		*meshbuffer_translucent;

	int			num_portal_opaque_meshes;
	int			num_portal_translucent_meshes;
} meshlist_t;

enum
{
	SKYBOX_RIGHT,
	SKYBOX_LEFT,
	SKYBOX_FRONT,
	SKYBOX_BACK,
	SKYBOX_TOP,
	SKYBOX_BOTTOM		// not used for skydome, but is used for skybox
};

typedef struct
{
	float			skyHeight;
	mesh_t			*meshes;
	vec2_t			*sphereStCoords[6];
	vec2_t			*linearStCoords[6];

	struct ref_shader_s		*farboxShaders[6];
	struct ref_shader_s		*nearboxShaders[6];
} skydome_t;

#endif /*__R_MESH_H__*/
