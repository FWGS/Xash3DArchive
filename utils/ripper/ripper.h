//=======================================================================
//			Copyright XashXT Group 2007 ©
//			ripper.h - Xash Miptex Decompiler
//=======================================================================
#ifndef BASECONVERTOR_H
#define BASECONVERTOR_H

#include "utils.h"

extern stdlib_api_t com;
extern byte *basepool;
extern string gs_gamedir;
#define Sys_Error com.error
extern uint app_name;
extern qboolean write_qscsript;
extern int game_family;

// common tools
qboolean Conv_CreateShader( const char *name, rgbdata_t *pic, const char *ext, const char *anim, int surf, int cnt );
qboolean Conv_CheckMap( const char *mapname ); // for detect gametype
qboolean Conv_CheckWad( const char *wadname ); // for detect gametype
qboolean MipExist( const char *name );

typedef enum
{
	GAME_GENERIC = 0,
	GAME_DOOM1,
	GAME_DOOM3,
	GAME_QUAKE1,
	GAME_QUAKE2,
	GAME_QUAKE3,
	GAME_QUAKE4,
	GAME_HALFLIFE,
	GAME_HALFLIFE2,
	GAME_HALFLIFE2_BETA,
	GAME_HEXEN2,
	GAME_XASH3D,
	GAME_RTCW,	
} game_family_t;

static const char *game_names[] =
{
	"Unknown",
	"Doom1\\Doom2",
	"Doom3\\Quake4",
	"Quake1",
	"Quake2",
	"Quake3",
	"Quake4",
	"Half-Life",
	"Half-Life 2",
	"Half-Life 2 Beta",
	"Nexen 2",
	"Xash 3D",
	"Return To Castle Wolfenstein"
};

//=====================================
//	convertor modules
//=====================================
qboolean ConvSPR( const char *name, byte *buffer, size_t filesize, const char *ext );// q1, q2, hl and spr32 sprites
qboolean ConvPCX( const char *name, byte *buffer, size_t filesize, const char *ext );// pcx images (use custom palette)
qboolean ConvFLT( const char *name, byte *buffer, size_t filesize, const char *ext );// Doom1 flat images (textures)
qboolean ConvFLP( const char *name, byte *buffer, size_t filesize, const char *ext );// Doom1 flat images (menu pics)
qboolean ConvJPG( const char *name, byte *buffer, size_t filesize, const char *ext );// Quake3 textures
qboolean ConvBMP( const char *name, byte *buffer, size_t filesize, const char *ext );// 8-bit maps with alpha-channel
qboolean ConvMIP( const char *name, byte *buffer, size_t filesize, const char *ext );// Quake1, Half-Life wad textures
qboolean ConvLMP( const char *name, byte *buffer, size_t filesize, const char *ext );// Quake1, Half-Life lump images
qboolean ConvFNT( const char *name, byte *buffer, size_t filesize, const char *ext );// Half-Life system fonts
qboolean ConvWAL( const char *name, byte *buffer, size_t filesize, const char *ext );// Quake2 textures
qboolean ConvVTF( const char *name, byte *buffer, size_t filesize, const char *ext );// Quake2 textures
qboolean ConvSKN( const char *name, byte *buffer, size_t filesize, const char *ext );// Doom1 sprite models
qboolean ConvBSP( const char *name, byte *buffer, size_t filesize, const char *ext );// Extract textures from bsp
qboolean ConvMID( const char *name, byte *buffer, size_t filesize, const char *ext );// Doom1 music files (midi)
qboolean ConvRAW( const char *name, byte *buffer, size_t filesize, const char *ext );// write file without converting
qboolean ConvDAT( const char *name, byte *buffer, size_t filesize, const char *ext );// quakec progs into source.qc

#endif//BASECONVERTOR_H