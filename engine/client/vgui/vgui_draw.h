//=======================================================================
//			Copyright XashXT Group 2011 ©
//		         vgui_draw.h - vgui draw methods
//=======================================================================
#ifndef VGUI_DRAW_H
#define VGUI_DRAW_H

#ifdef __cplusplus
extern "C" {
#endif

#define VGUI_MAX_TEXTURES	2048	// a half of total textures count

// VGUI generic vertex
typedef struct
{
	vec2_t	point;
	vec2_t	coord;
} vpoint_t;

//
// vgui_backend.c
//

void VGUI_DrawInit( void );
void VGUI_SetupDrawingText( byte *pColor );
void VGUI_SetupDrawingRect( byte *pColor );
void VGUI_SetupDrawingImage( void );
void VGUI_BindTexture( int id );
void VGUI_EnableTexture( qboolean enable );
void VGUI_CreateTexture( int id, int width, int height );
void VGUI_UploadTexture( int id, const char *buffer, int width, int height );
void VGUI_UploadTextureBlock( int id, int drawX, int drawY, const byte *rgba, int blockWidth, int blockHeight );
void VGUI_DrawQuad( const vpoint_t *ul, const vpoint_t *lr );
void VGUI_GetTextureSizes( int *width, int *height );
int VGUI_GenerateTexture( void );

#ifdef __cplusplus
void EnableScissor( qboolean enable );
void SetScissorRect( int left, int top, int right, int bottom );
qboolean ClipRect( const vpoint_t &inUL, const vpoint_t &inLR, vpoint_t *pOutUL, vpoint_t *pOutLR );
#endif

#ifdef __cplusplus
}
#endif
#endif//VGUI_DRAW_H