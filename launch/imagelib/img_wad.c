//=======================================================================
//			Copyright XashXT Group 2007 �
//			img_mip.c - hl1 q1 image mips
//=======================================================================

#include "imagelib.h"
#include "wadfile.h"
#include "studio.h"
#include "sprite.h"
#include "qfont.h"

/*
============
Image_LoadPAL
============
*/
qboolean Image_LoadPAL( const char *name, const byte *buffer, size_t filesize )
{
	int	rendermode = LUMP_NORMAL; 

	if( filesize != 768 )
	{
		MsgDev( D_ERROR, "Image_LoadPAL: (%s) have invalid size (%d should be %d)\n", name, filesize, 768 );
		return false;
	}

	if( name[0] == '#' )
	{
		// using palette name as rendermode
		if( com.stristr( name, "normal" ))
			rendermode = LUMP_NORMAL;
		else if( com.stristr( name, "transparent" ))
			rendermode = LUMP_TRANSPARENT;
		else if( com.stristr( name, "decal" ))
			rendermode = LUMP_DECAL;
		else if( com.stristr( name, "indexalpha" ))
			rendermode = LUMP_INDEXALPHA;
		else if( com.stristr( name, "qfont" ))
			rendermode = LUMP_QFONT;
		else if( com.stristr( name, "valve" ))
			buffer = NULL; // force to get HL palette
	}

	// NOTE: image.d_currentpal not cleared with Image_Reset()
	// and stay valid any time before new call of Image_SetPalette
	Image_GetPaletteLMP( buffer, rendermode );
	Image_CopyPalette32bit();

	image.rgba = NULL;	// only palette, not real image
	image.size = 1024;	// expanded palette
	image.width = image.height = 0;
	
	return true;
}

/*
============
Image_LoadFNT
============
*/
qboolean Image_LoadFNT( const char *name, const byte *buffer, size_t filesize )
{
	qfont_t		font;
	const byte	*pal, *fin;
	size_t		size;
	int		numcolors;

	if( image.hint == IL_HINT_Q1 )
		return false;	// Quake1 doesn't have qfonts

	if( filesize < sizeof( font ))
		return false;
	Mem_Copy( &font, buffer, sizeof( font ));
	
	// last sixty four bytes - what the hell ????
	size = sizeof( qfont_t ) - 4 + ( 128 * font.width * QCHAR_WIDTH ) + sizeof( short ) + 768 + 64;

	if( size != filesize )
	{
		// oldstyle font: "conchars" or "creditsfont"
		image.width = 256;		// hardcoded
		image.height = font.height;
	}
	else
	{
		// Half-Life 1.1.0.0 font style (qfont_t)
		image.width = font.width * QCHAR_WIDTH;
		image.height = font.height;
	}

	if(!Image_LumpValidSize( name )) return false;
	fin = buffer + sizeof( font ) - 4;

	pal = fin + (image.width * image.height);
	numcolors = *(short *)pal, pal += sizeof( short );
	image.flags |= IMAGE_HAS_ALPHA; // fonts always have transparency

	if( numcolors == 768 )
	{
		// newstyle font
		Image_GetPaletteLMP( pal, LUMP_QFONT );
	}
	else if( numcolors == 256 )
	{
		// oldstyle font
		Image_GetPaletteLMP( pal, LUMP_TRANSPARENT );
	}
	else 
	{
		if( image.hint == IL_HINT_NO )
			MsgDev( D_ERROR, "Image_LoadFNT: (%s) have invalid palette size %d\n", name, numcolors );
		return false;
	}

	image.type = PF_INDEXED_32;	// 32-bit palette

	return Image_AddIndexedImageToPack( fin, image.width, image.height );
}

/*
============
Image_LoadMDL
============
*/
qboolean Image_LoadMDL( const char *name, const byte *buffer, size_t filesize )
{
	byte		*fin;
	size_t		pixels;
	mstudiotexture_t	*pin;
	int		i, flags;

	pin = (mstudiotexture_t *)buffer;
	flags = pin->flags;

	image.width = pin->width;
	image.height = pin->height;
	pixels = image.width * image.height;
	fin = (byte *)pin->index;	// setup buffer

	if(!Image_LumpValidSize( name )) return false;

	if( image.hint != IL_HINT_Q1 && !( flags & STUDIO_NF_QUAKESKIN ))
	{
		if( filesize < ( sizeof( *pin ) + pixels + 768 ))
			return false;

		if( flags & STUDIO_NF_TRANSPARENT )
		{
			byte	*pal = fin + pixels;

			// make transparent color is black, blue color looks ugly
			if( Sys.app_name == HOST_NORMAL )
				pal[255*3+0] = pal[255*3+1] = pal[255*3+2] = 0;

			Image_GetPaletteLMP( pal, LUMP_TRANSPARENT );
			image.flags |= IMAGE_HAS_ALPHA;
		}
		else Image_GetPaletteLMP( fin + pixels, LUMP_NORMAL );
	}
	else if( image.hint != IL_HINT_HL && flags & STUDIO_NF_QUAKESKIN )
	{
		if( filesize < ( sizeof( *pin ) + pixels ))
			return false;

		// alias models setup
		Image_GetPaletteQ1();

		// check for luma pixels
		for( i = 0; i < pixels; i++ )
		{
			if( fin[i] > 224 )
			{
				Msg( "%s has luma-pixels\n", name );
				image.flags |= IMAGE_HAS_LUMA_Q1;
				break;
			}
		}
	}
	else
	{
		if( image.hint == IL_HINT_NO )
			MsgDev( D_ERROR, "Image_LoadMDL: lump (%s) is corrupted\n", name );
		return false; // unknown or unsupported mode rejected
	} 

	image.type = PF_INDEXED_32;	// 32-bit palete

	return Image_AddIndexedImageToPack( fin, image.width, image.height );
}

/*
============
Image_LoadSPR
============
*/
qboolean Image_LoadSPR( const char *name, const byte *buffer, size_t filesize )
{
	dspriteframe_t	*pin;	// indetical for q1\hl sprites

	if( image.hint == IL_HINT_HL )
	{
		if( !image.d_currentpal )
		{
			MsgDev( D_ERROR, "Image_LoadSPR: (%s) palette not installed\n", name );
			return false;		
		}
	}
	else if( image.hint == IL_HINT_Q1 )
	{
		Image_GetPaletteQ1();
	}
	else return false; // unknown mode rejected

	pin = (dspriteframe_t *)buffer;
	image.width = pin->width;
	image.height = pin->height;

	if( filesize < image.width * image.height )
	{
		MsgDev( D_ERROR, "Image_LoadSPR: file (%s) have invalid size\n", name );
		return false;
	}

	// sorry, can't validate palette rendermode
	if(!Image_LumpValidSize( name )) return false;
	image.type = PF_INDEXED_32;	// 32-bit palete

	// detect alpha-channel by palette type
	if( image.d_rendermode == LUMP_INDEXALPHA || image.d_rendermode == LUMP_TRANSPARENT )
		image.flags |= IMAGE_HAS_ALPHA;

	// make transparent color is black, blue color looks ugly
	if( image.d_rendermode == LUMP_TRANSPARENT && Sys.app_name == HOST_NORMAL )
		image.d_currentpal[255] = 0;

	return Image_AddIndexedImageToPack( (byte *)(pin + 1), image.width, image.height );
}

/*
==============
Image_LoadWAL
==============
*/
qboolean Image_LoadWAL( const char *name, const byte *buffer, size_t filesize )
{
	wal_t 	wal;
	int	pixels, ofs[4], mipsize;
	int	i, flags, value, contents; // wal additional parms
	const byte *fin;

	if( filesize < (int)sizeof( wal ))
	{
		MsgDev( D_ERROR, "Image_LoadWAL: file (%s) have invalid size\n", name );
		return false;
	}
	Mem_Copy( &wal, buffer, sizeof( wal ));

	flags = wal.flags;
	value = wal.value;
	contents = wal.contents;
	image.width = wal.width;
	image.height = wal.height;

	Mem_Copy( ofs, wal.offsets, sizeof( ofs ));

	if( !Image_LumpValidSize( name ))
		return false;

	pixels = image.width * image.height;
	mipsize = (int)sizeof(wal) + ofs[0] + pixels;
	if( pixels > 256 && filesize < mipsize )
	{
		// NOTE: wal's with dimensions < 32 have invalid sizes.
		MsgDev( D_ERROR, "Image_LoadWAL: file (%s) have invalid size\n", name );
		return false;
	}

	image.type = PF_INDEXED_32;	// 32-bit palete
	fin = buffer + ofs[0];

	// check for luma pixels
	for( i = 0; i < image.width * image.height; i++ )
	{
		if( fin[i] > 208 && fin[i] < 240 )
		{
			image.flags |= IMAGE_HAS_LUMA_Q2;
			break;
		}
	}

	Image_GetPaletteQ2(); // hardcoded
	return Image_AddIndexedImageToPack( fin, image.width, image.height );
}

/*
============
Image_LoadFLT
============
*/
qboolean Image_LoadFLT( const char *name, const byte *buffer, size_t filesize )
{
	flat_t	flat;
	vfile_t	*f;
	qboolean	result = false;
	int	trans_pixels = 0;
	word	column_loop, row_loop;
	int	i, column_offset, pointer_position, first_pos;
	byte	*Data, post, topdelta, length;

	if( filesize < (int)sizeof( flat ))
	{
		MsgDev( D_ERROR, "Image_LoadFLAT: file (%s) have invalid size\n", name );
		return false;
	}

	// stupid copypaste from DevIL, but it works
	f = VFS_Create( buffer, filesize );
	first_pos = VFS_Tell( f );
	VFS_Read(f, &flat, sizeof( flat ));

	image.width  = flat.width;
	image.height = flat.height;

	if( !Image_LumpValidSize( name ))
		return false;

	Data = (byte *)Mem_Alloc( Sys.imagepool, image.width * image.height );
	Mem_Set( Data, 247, image.width * image.height ); // set default transparency

	for( column_loop = 0; column_loop < image.width; column_loop++ )
	{
		VFS_Read(f, &column_offset, sizeof(int));
		pointer_position = VFS_Tell( f );
		VFS_Seek( f, first_pos + column_offset, SEEK_SET );

		while( 1 )
		{
			if(VFS_Read(f, &topdelta, 1) != 1) goto img_trunc;
			if( topdelta == 255 ) break;
			if(VFS_Read(f, &length, 1) != 1) goto img_trunc;
			if(VFS_Read(f, &post, 1) != 1) goto img_trunc;

			for( row_loop = 0; row_loop < length; row_loop++ )
			{
				if(VFS_Read(f, &post, 1) != 1) goto img_trunc;
				if(row_loop + topdelta < image.height)
					Data[(row_loop + topdelta) * image.width + column_loop] = post;
			}
			VFS_Read( f, &post, 1 );
		}
		VFS_Seek( f, pointer_position, SEEK_SET );
	}
	VFS_Close( f );

	// swap colors in image, and check for transparency
	for( i = 0; i < image.width * image.height; i++ )
	{
		if( Data[i] == 247 )
		{
			Data[i] = 255;
			trans_pixels++;
		}
		else if( Data[i] == 255 ) Data[i] = 247;
	}

	// yes it's really transparent texture
	// otherwise transparency it's a product of lazy designers (or painters ?)
	if( trans_pixels > TRANS_THRESHOLD ) image.flags |= IMAGE_HAS_ALPHA;

	image.type = PF_INDEXED_32;	// 32-bit palete
	Image_GetPaletteD1();

	result = Image_AddIndexedImageToPack( Data, image.width, image.height );
	if( Data ) Mem_Free( Data );
	return result;
img_trunc:
	VFS_Close( f );
	Mem_Free( Data );
	MsgDev( D_NOTE, "Image_LoadFLAT: probably it's not a .flat image)\n" );	
	return false;
}

/*
============
Image_LoadLMP
============
*/
qboolean Image_LoadLMP( const char *name, const byte *buffer, size_t filesize )
{
	lmp_t	lmp;
	byte	*fin, *pal;
	int	rendermode;
	int	pixels;

	if( filesize < sizeof( lmp ))
	{
		MsgDev( D_ERROR, "Image_LoadLMP: file (%s) have invalid size\n", name );
		return false;
	}

	// greatest hack from valve software (particle palette)
	if( com.stristr( name, "palette.lmp" ))
		return Image_LoadPAL( name, buffer, filesize );

	// greatest hack from id software
	if( image.hint != IL_HINT_HL && com.stristr( name, "conchars" ))
	{
		image.width = image.height = 128;
		image.flags |= IMAGE_HAS_ALPHA;
		rendermode = LUMP_QFONT;
		filesize += sizeof(lmp);
		fin = (byte *)buffer;
	}
	else
	{
		fin = (byte *)buffer;
		Mem_Copy( &lmp, fin, sizeof( lmp ));
		image.width = lmp.width;
		image.height = lmp.height;
		rendermode = LUMP_NORMAL;
		fin += sizeof(lmp);
	}
	pixels = image.width * image.height;

	if( filesize < sizeof( lmp ) + pixels )
	{
		MsgDev( D_ERROR, "Image_LoadLMP: file (%s) have invalid size %d\n", name, filesize );
		return false;
	}

	if( !Image_ValidSize( name ))
		return false;         

	if( image.hint != IL_HINT_Q1 && filesize > (int)sizeof(lmp) + pixels )
	{
		int	numcolors;

		pal = fin + pixels;
		numcolors = *(short *)pal;
		if( numcolors != 256 ) pal = NULL; // corrupted lump ?
		else pal += sizeof( short );
	}
	else if( image.hint != IL_HINT_HL ) pal = NULL;
	else return false; // unknown mode rejected
	if( fin[0] == 255 ) image.flags |= IMAGE_HAS_ALPHA;

	Image_GetPaletteLMP( pal, rendermode );
	image.type = PF_INDEXED_32;	// 32-bit palete
	return Image_AddIndexedImageToPack( fin, image.width, image.height );
}

/*
=============
Image_LoadMIP
=============
*/
qboolean Image_LoadMIP( const char *name, const byte *buffer, size_t filesize )
{
	mip_t	mip;
	byte	*fin, *pal;
	int	ofs[4], rendermode;
	int	i, pixels, numcolors;

	if( filesize < sizeof( mip ))
	{
		MsgDev( D_ERROR, "Image_LoadMIP: file (%s) have invalid size\n", name );
		return false;
	}

	Mem_Copy( &mip, buffer, sizeof( mip ));
	image.width = mip.width;
	image.height = mip.height;

	if( !Image_ValidSize( name ))
		return false;

	Mem_Copy( ofs, mip.offsets, sizeof( ofs ));
	pixels = image.width * image.height;

	if( image.hint != IL_HINT_Q1 && filesize >= (int)sizeof(mip) + ((pixels * 85)>>6) + sizeof(short) + 768)
	{
		// half-life 1.0.0.1 mip version with palette
		fin = (byte *)buffer + mip.offsets[0];
		pal = (byte *)buffer + mip.offsets[0] + (((image.width * image.height) * 85)>>6);
		numcolors = *(short *)pal;
		if( numcolors != 256 ) pal = NULL; // corrupted mip ?
		else pal += sizeof( short ); // skip colorsize 

		// detect rendermode
		if( com.strrchr( name, '{' ))
		{
			color24	*col = (color24 *)pal;

			// check for grayscale palette
			for( i = 0; i < 255; i++, col++ )
			{
				if( col->r != col->g || col->g != col->b )
					break;
			}

			if( i != 255 )
			{
				rendermode = LUMP_TRANSPARENT;

				// make transparent color is black, blue color looks ugly
				if( Sys.app_name == HOST_NORMAL )
					pal[255*3+0] = pal[255*3+1] = pal[255*3+2] = 0;
			}
			else
			{
				// apply decal palette immediately
				image.flags |= IMAGE_COLORINDEX;
				if( Sys.app_name == HOST_NORMAL )
					rendermode = LUMP_DECAL;
				else rendermode = LUMP_TRANSPARENT;
			}
			image.flags |= IMAGE_HAS_ALPHA;
		}
		else
		{
			int	pal_type;

			// NOTE: we can have luma-pixels if quake1/2 texture
			// converted into the hl texture but palette leave unchanged
			// this is a good reason for using fullbright pixels
			pal_type = Image_ComparePalette( pal );

			// check for luma pixels
			switch( pal_type )
			{
			case PAL_QUAKE1:
				for( i = 0; i < image.width * image.height; i++ )
				{
					if( fin[i] > 224 )
					{
						image.flags |= IMAGE_HAS_LUMA_Q1;
						break;
					}
				}
				break;
			case PAL_QUAKE2:
				for( i = 0; i < image.width * image.height; i++ )
				{
					if( fin[i] > 208 && fin[i] < 240 )
					{
						image.flags |= IMAGE_HAS_LUMA_Q2;
						break;
					}
				}
				break;
			}
			rendermode = LUMP_NORMAL;
		}

		Image_GetPaletteLMP( pal, rendermode );
	}
	else if( image.hint != IL_HINT_HL && filesize >= (int)sizeof(mip) + ((pixels * 85)>>6))
	{
		// quake1 1.01 mip version without palette
		fin = (byte *)buffer + mip.offsets[0];
		pal = NULL; // clear palette
		rendermode = LUMP_NORMAL;

		// check for luma pixels
		for( i = 0; i < image.width * image.height; i++ )
		{
			if( fin[i] > 224 )
			{
				// don't apply luma to water surfaces because
				// we use glpoly->next for store luma chain each frame
				// and can't modify glpoly_t because many-many HL mods
				// expected unmodified glpoly_t and can crashes on changed struct
				// water surfaces uses glpoly->next as pointer to subdivided surfaces (as q1)
				if( mip.name[0] != '*' && mip.name[0] != '!' )
					image.flags |= IMAGE_HAS_LUMA_Q1;
				break;
			}
		}

		Image_GetPaletteQ1();
	}
	else
	{
		if( image.hint == IL_HINT_NO )
			MsgDev( D_ERROR, "Image_LoadMIP: lump (%s) is corrupted\n", name );
		return false; // unknown or unsupported mode rejected
	} 

	// check for quake-sky texture
	if( !com.strncmp( mip.name, "sky", 3 ) && image.width == ( image.height * 2 ))
	{
		// FIXME: run additional checks for palette type and colors ?
		image.flags |= IMAGE_QUAKESKY;
	}

	image.type = PF_INDEXED_32;	// 32-bit palete
	return Image_AddIndexedImageToPack( fin, image.width, image.height );
}