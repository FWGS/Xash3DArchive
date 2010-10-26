//=======================================================================
//			Copyright XashXT Group 2008 ©
//		  triangle_api.h - custom triangles rendering
//=======================================================================
#ifndef TRIANGLE_API_H
#define TRIANGLE_API_H

#define TRI_API_VERSION	2

typedef enum 
{
	TRI_FRONT = 0,
	TRI_NONE,
} TRI_CULL;

#define TRI_TRIANGLES	0
#define TRI_TRIANGLE_FAN	1
#define TRI_QUADS		2
#define TRI_POLYGON		3
#define TRI_LINES		4	
#define TRI_TRIANGLE_STRIP	5
#define TRI_QUAD_STRIP	6	// UNDONE: not implemented 

typedef enum
{
	TRI_SHADER = 1,
	TRI_MAXCAPS
} TRI_CAPS;

typedef struct triapi_s
{
	int	version;

	int	(*LoadShader)( const char *szShaderName, int fShaderNoMip );
	int	(*GetSpriteTexture)( int spriteIndex, int spriteFrame );
	void	(*RenderMode)( int mode );
	void	(*Begin)( int primitiveCode );
	void	(*End)( void );

	void	(*Enable)( int cap );
	void	(*Disable)( int cap );
	void	(*Vertex3f)( float x, float y, float z );
	void	(*Vertex3fv)( const float *v );
	void	(*Normal3f)( float x, float y, float z );
	void	(*Normal3fv)( const float *v );
	void	(*Color4f)( float r, float g, float b, float a );
	void	(*Color4ub)( byte r, byte g, byte b, byte a );
	void	(*TexCoord2f)( float u, float v );
	void	(*Bind)( int shader, int frame );	// use handle that return pfnLoadShader
	void	(*CullFace)( TRI_CULL mode );
	void	(*ScreenToWorld)( float *screen, float *world  ); 
	int	(*WorldToScreen)( float *world, float *screen );  // returns 1 if it's z clipped
	void	(*Fog)( float flFogColor[3], float flStart, float flEnd, int bOn );
} triapi_t;

#endif//TRIANGLE_API_H