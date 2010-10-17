//=======================================================================
//			Copyright XashXT Group 2010 ©
//		     custom.h - create custom user recources
//=======================================================================
#ifndef CUSTOM_H
#define CUSTOM_H

#include "const.h"

/////////////////
// Customization
// passed to pfnPlayerCustomization
// For automatic downloading.

typedef enum
{
	t_sound = 0,
	t_skin,
	t_model,
	t_decal,
	t_generic,
	t_eventscript,
	t_world,			// fake type for world, is really t_model
} resourcetype_t;

typedef struct
{
	int		size;
} _resourceinfo_t;

typedef struct resourceinfo_s
{
	_resourceinfo_t	info[8];
} resourceinfo_t;

#define RES_FATALIFMISSING	(1<<0)	// Disconnect if we can't get this file.
#define RES_WASMISSING	(1<<1)	// Do we have the file locally, did we get it ok?
#define RES_CUSTOM		(1<<2)	// Is this resource one that corresponds to another player's customization
				// or is it a server startup resource.
#define RES_REQUESTED	(1<<3)	// Already requested a download of this one
#define RES_PRECACHED	(1<<4)	// Already precached

typedef struct resource_s
{
	char			szFileName[64];	// file name to download/precache.
	resourcetype_t		type;		// t_sound, t_skin, t_model, t_decal.
	int			nIndex;		// for t_decals
	int			nDownloadSize;	// size in bytes if this must be downloaded.
	byte			ucFlags;

	// for handling client to client resource propagation
	byte			rgucMD5_hash[16];	// to determine if we already have it.
	byte			playernum;	// which player index this resource is associated with,
						// if it's a custom resource.

	byte			rguc_reserved[32];	// for future expansion
	struct resource_s		*pNext;		// Next in chain.
	struct resource_s		*pPrev;
} resource_t;

typedef struct custom_s
{
	BOOL			bInUse;		// is this customization in use;
	resource_t		resource;		// the resource_t for this customization
	BOOL			bTranslated;	// has the raw data been translated into a useable format?  
						// (e.g., raw decal .wad make into texture_t *)
	int			nUserData1;	// sustomization specific data
	int			nUserData2;	// customization specific data
	void			*pInfo;		// buffer that holds the data structure that references
						// the data (e.g., the cachewad_t)
	void			*pBuffer;		// buffer that holds the data for the customization
						// (the raw .wad data)
	struct customization_s	*pNext;		// next in chain
} customization_t;

#endif // CUSTOM_H