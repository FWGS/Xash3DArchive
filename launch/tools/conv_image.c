//=======================================================================
//			Copyright XashXT Group 2008 ©
//		     conv_image.c - convert various image type
//=======================================================================

#include "ripper.h"

// doom spritemodel_qc
typedef struct angled_s
{
	char	name[10];		// copy of skin name

	int	width;		// lumpwidth
	int	height;		// lumpheight
	int	origin[2];	// monster origin
	byte	xmirrored;	// swap left and right
} angled_t;

struct angledframe_s
{
	angled_t	frame[8];		// angled group or single frame
	int	bounds[2];	// group or frame maxsizes
	byte	angledframes;	// count of angled frames max == 8
	byte	normalframes;	// count of anim frames max == 1
	byte	mirrorframes;	// animation mirror stored

	char	membername[8];	// current model name, four characters
	char	animation;	// current animation number
	qboolean	in_progress;	// current state
	file_t	*f;		// skin script
} flat;

static void Skin_RoundDimensions( int *scaled_width, int *scaled_height )
{
	int width, height;

	for( width = 1; width < *scaled_width; width <<= 1 );
	for( height = 1; height < *scaled_height; height <<= 1 );

	*scaled_width = bound( 1, width, 512 );
	*scaled_height = bound( 1, height, 512 );
}

void Skin_WriteSequence( void )
{
	int	i;

	Skin_RoundDimensions( &flat.bounds[0], &flat.bounds[1] );

	// time to dump frames :)
	if( flat.angledframes == 8 )
	{
		// angled group is full, dump it!
		FS_Print( flat.f, "\n$angled\n{\n" );
		FS_Printf( flat.f, "\t// frame '%c'\n", flat.frame[0].name[4] );
		FS_Printf( flat.f, "\t$resample\t\t%d %d\n", flat.bounds[0], flat.bounds[1] );
		for( i = 0; i < 8; i++)
		{
			FS_Printf( flat.f,"\t$load\t\t%s.bmp", flat.frame[i].name );
			if( flat.frame[i].xmirrored ) FS_Print( flat.f," flip_x\n"); 
			else FS_Print( flat.f, "\n" );
			FS_Printf( flat.f,"\t$frame\t\t0 0 %d %d", flat.frame[i].width, flat.frame[i].height );
			FS_Printf( flat.f, " 0.1 %d %d\n", flat.frame[i].origin[0], flat.frame[i].origin[1] );
		}
		FS_Print( flat.f, "}\n" );
	}
	else if( flat.normalframes == 1 )
	{
		// single frame stored
		FS_Print( flat.f, "\n" );
		FS_Printf( flat.f, "// frame '%c'\n", flat.frame[0].name[4] );
		FS_Printf( flat.f,"$resample\t\t%d %d\n", flat.bounds[0], flat.bounds[1] );
		FS_Printf( flat.f,"$load\t\t%s.bmp\n", flat.frame[0].name );
		FS_Printf( flat.f,"$frame\t\t0 0 %d %d", flat.frame[0].width, flat.frame[0].height );
		FS_Printf( flat.f, " 0.1 %d %d\n", flat.frame[0].origin[0], flat.frame[0].origin[1]);
	}

	// drop mirror frames too
	if( flat.mirrorframes == 8 )
	{
		// mirrored group is always flipped
		FS_Print( flat.f, "\n$angled\n{\n" );
		FS_Printf( flat.f, "\t//frame '%c' (mirror '%c')\n", flat.frame[0].name[6], flat.frame[0].name[4] );
		FS_Printf( flat.f, "\t$resample\t\t%d %d\n", flat.bounds[0], flat.bounds[1] );
		for( i = 2; i > -1; i-- )
		{
			FS_Printf( flat.f,"\t$load\t\t%s.bmp flip_x\n", flat.frame[i].name );
			FS_Printf( flat.f,"\t$frame\t\t0 0 %d %d", flat.frame[i].width, flat.frame[i].height );
			FS_Printf( flat.f, " 0.1 %d %d\n", flat.frame[i].origin[0], flat.frame[i].origin[1] );
		}
		for( i = 7; i > 2; i-- )
		{
			FS_Printf( flat.f,"\t$load\t\t%s.bmp flip_x\n", flat.frame[i].name );
			FS_Printf( flat.f,"\t$frame\t\t0 0 %d %d", flat.frame[i].width, flat.frame[i].height );
			FS_Printf( flat.f, " 0.1 %d %d\n", flat.frame[i].origin[0], flat.frame[i].origin[1] );
		}
		FS_Print( flat.f, "}\n" );
	}

	flat.bounds[0] = flat.bounds[1] = 0;
	Mem_Set( &flat.frame, 0, sizeof( flat.frame )); 
	flat.angledframes = flat.normalframes = flat.mirrorframes = 0; // clear all
}

void Skin_FindSequence( const char *name, rgbdata_t *pic )
{
	uint	headlen;
	char	num, header[10];

	// create header from flat name
	com.strncpy( header, name, 10 );
	headlen = com.strlen( name );

	if( flat.animation != header[4] )
	{
		// write animation
		Skin_WriteSequence();
		flat.animation = header[4];
	}

	if( flat.animation == header[4] )
	{
		// update bounds
		if( flat.bounds[0] < pic->width ) flat.bounds[0] = pic->width;
		if( flat.bounds[1] < pic->height) flat.bounds[1] = pic->height;

		// continue collect frames
		if( headlen == 6 )
		{
			num = header[5] - '0';
			if(num == 0) flat.normalframes++;		// animation frame
			if(num == 8) num = 0;			// merge 
			flat.angledframes++; 			// angleframe stored
			com.strncpy( flat.frame[num].name, header, 9 );
			flat.frame[num].width = pic->width;
			flat.frame[num].height = pic->height;
			flat.frame[num].origin[0] = pic->width>>1;	// center
			flat.frame[num].origin[1] = pic->height;	// floor
			flat.frame[num].xmirrored = false;
		}
		else if( headlen == 8 )
		{
			// normal image
			num = header[5] - '0';
			if(num == 8) num = 0;  			// merge 
			com.strncpy( flat.frame[num].name, header, 9 );
			flat.frame[num].width = pic->width;
			flat.frame[num].height = pic->height;
			flat.frame[num].origin[0] = pic->width>>1;	// center
			flat.frame[num].origin[1] = pic->height;	// floor
			flat.frame[num].xmirrored = false;
			flat.angledframes++;			// frame stored

			if( header[4] != header[6] )
			{
				// mirrored groups
				flat.mirrorframes++;
				return;
			}

			// mirrored image
			num = header[7] - '0';			// angle it's a direct acess to group
			if(num == 8) num = 0;			// merge 
			com.strncpy( flat.frame[num].name, header, 9 );
			flat.frame[num].width = pic->width;
			flat.frame[num].height = pic->height;
			flat.frame[num].origin[0] = pic->width>>1;	// center
			flat.frame[num].origin[1] = pic->height;	// floor
			flat.frame[num].xmirrored = true;		// it's mirror frame
			flat.angledframes++;			// frame stored
		}
		else Sys_Break( "Skin_CreateScript: invalid name %s\n", name );	// this never happens
	}
}

void Skin_ProcessScript( const char *wad, const char *name )
{
	if( flat.in_progress )
	{
		// finish script
		Skin_WriteSequence();
		FS_Close( flat.f );
		flat.in_progress = false;
	}
	if( !flat.in_progress )
	{
		// start from scratch
		com.strncpy( flat.membername, name, 5 );		
		flat.f = FS_Open( va( "%s/%s/%s.qc", gs_gamedir, wad, flat.membername ), "w" );
		flat.in_progress = true;
		flat.bounds[0] = flat.bounds[1] = 0;

		// write description
		FS_Print( flat.f,"//=======================================================================\n");
		FS_Printf( flat.f,"//\t\t\tCopyright XashXT Group %s ©\n", timestamp( TIME_YEAR_ONLY ));
		FS_Print( flat.f,"//\t\t\twritten by Xash Miptex Decompiler\n");
		FS_Print( flat.f,"//=======================================================================\n");

		// write sprite header
		FS_Printf( flat.f, "\n$spritename\t%s.spr\n", flat.membername );
		FS_Print( flat.f,  "$type\t\tfacing_upright\n"  ); // constant
		FS_Print( flat.f,  "$texture\t\talphatest\n");
		FS_Print( flat.f,  "$noresample\n" );		// comment this command by taste
	}
}

// close sequence for unexpected concessions
void Skin_FinalizeScript( void )
{
	if( !flat.in_progress ) return;

	// finish script
	Skin_WriteSequence();
	FS_Close( flat.f );
	flat.in_progress = false;
}

void Skin_CreateScript( const char *name, rgbdata_t *pic )
{
	string	skinname, wadname;

	FS_ExtractFilePath( name, wadname );	// wad name
	FS_FileBase( name, skinname );	// skinname

	if(com.strnicmp( skinname, flat.membername, 4 ))
          	Skin_ProcessScript( wadname, skinname );

	if( flat.in_progress )
		Skin_FindSequence( skinname, pic );
}

void Conv_WriteQCScript( const char *imagepath, rgbdata_t *p )
{
	file_t	*f = NULL;
	string	qcname, qcpath;
	string	temp, lumpname;

	// write also wadlist.qc for xwad compiler
	FS_ExtractFilePath( imagepath, temp );
	FS_FileBase( imagepath, lumpname );
	FS_FileBase( temp, qcname );
	FS_DefaultExtension( qcname, ".qc" );
	com.snprintf( qcpath, MAX_STRING, "%s/%s/$%s", gs_gamedir, temp, qcname );

	if( write_qscsript )
	{	
		if( FS_FileExists( qcpath ))
		{
			script_t	*test;
			token_t	token;

			// already exist, search for current name
			test = Com_OpenScript( qcpath, NULL, 0 );
			while( Com_ReadToken( test, SC_ALLOW_NEWLINES|SC_PARSE_GENERIC, &token ))
			{
				if( !com.stricmp( token.string, "$mipmap" ))
				{
					Com_ReadToken( test, SC_PARSE_GENERIC, &token );
					if( !com.stricmp( token.string, lumpname ))
					{
						Com_CloseScript( test );
						return; // already exist
					}
				}
				Com_SkipRestOfLine( test );
			}
			Com_CloseScript( test );
			f = FS_Open( qcpath, "a" );	// append
		}
		else
		{
			FS_StripExtension( qcname ); // no need anymore
			f = FS_Open( qcpath, "w" );	// new file
			// write description
			FS_Print(f,"//=======================================================================\n");
			FS_Printf(f,"//\t\t\tCopyright XashXT Group %s ©\n", timestamp( TIME_YEAR_ONLY ));
			FS_Print(f,"//\t\t\twritten by Xash Miptex Decompiler\n");
			FS_Print(f,"//=======================================================================\n");
			FS_Printf(f,"$wadname\t%s.wad\n\n", qcname );
		}

		if( f && p )
		{
			if( !com.stricmp( FS_FileExtension( imagepath ), "lmp" ))
				FS_Printf( f,"$gfxpic\t%s\t0 0 %d %d\n", lumpname, p->width, p->height );
			else FS_Printf( f,"$mipmap\t%s\t0 0 %d %d\n", lumpname, p->width, p->height );
			if( p->flags & IMAGE_HAS_LUMA ) // also add luma image if present
				FS_Printf( f,"$mipmap\t%s_luma\t0 0 %d %d\n", lumpname, p->width, p->height );
			FS_Close( f ); // all done		
		}
	}
}

/*
============
ConvSKN
============
*/
qboolean ConvSKN( const char *name, byte *buffer, size_t filesize, const char *ext )
{
	rgbdata_t *pic = FS_LoadImage( va( "#%s.flt", name ), buffer, filesize );

	if( pic )
	{
		FS_SaveImage( va("%s/%s.%s", gs_gamedir, name, ext ), pic );
		Skin_CreateScript( name, pic );
		Msg( "%s.flat\n", name ); // echo to console
		FS_FreeImage( pic );
		return true;
	}
	return false;
}

/*
============
ConvFLP
============
*/
qboolean ConvFLP( const char *name, byte *buffer, size_t filesize, const char *ext )
{
	rgbdata_t *pic = FS_LoadImage( va( "#%s.flt", name ), buffer, filesize );

	if( pic )
	{
		FS_SaveImage(va("%s/%s.%s", gs_gamedir, name, ext ), pic );
		Msg( "%s.flat\n", name ); // echo to console
		FS_FreeImage( pic );
		return true;
	}
	return false;
}

/*
============
ConvFLT
============
*/
qboolean ConvFLT( const char *name, byte *buffer, size_t filesize, const char *ext )
{
	rgbdata_t *pic = FS_LoadImage( va( "#%s.flt", name ), buffer, filesize );

	if( pic )
	{
		string	savedname, tempname, path;
	
		if( pic->flags & IMAGE_HAS_ALPHA )
		{
			// insert '{' symbol for transparency textures
			FS_ExtractFilePath( name, path );
			FS_FileBase( name, tempname );
			com.snprintf( savedname, MAX_STRING, "%s/{%s", path, tempname );
		}
		else com.strncpy( savedname, name, MAX_STRING );

		FS_SaveImage( va("%s/%s.%s", gs_gamedir, savedname, ext ), pic );
		Conv_WriteQCScript( savedname, pic );
		Msg( "%s.flat\n", savedname ); // echo to console
		FS_FreeImage( pic );
		return true;
	}
	return false;
}

/*
============
ConvWAL
============
*/
qboolean ConvWAL( const char *name, byte *buffer, size_t filesize, const char *ext )
{
	rgbdata_t	*pic = FS_LoadImage( va( "#%s.wal", name ), buffer, filesize );

	if( pic )
	{
		FS_SaveImage( va("%s/%s.%s", gs_gamedir, name, ext ), pic );
		Conv_WriteQCScript( name, pic );
		Msg("%s.wal\n", name ); // echo to console
		FS_FreeImage( pic );
		return true;
	}
	return false;
}

/*
=============
ConvBMP
=============
*/
qboolean ConvBMP( const char *name, byte *buffer, size_t filesize, const char *ext )
{
	rgbdata_t	*pic = FS_LoadImage( va( "#%s.bmp", name ), buffer, filesize );

	if( pic )
	{
		FS_SaveImage( va("%s/%s.%s", gs_gamedir, name, ext ), pic );
		Conv_WriteQCScript( name, pic );
		Msg( "%s.bmp\n", name, ext ); // echo to console
		FS_FreeImage( pic );
		return true;
	}
	return false;
}

/*
=============
ConvPCX

this also uses by SP2_ConvertFrame
=============
*/
qboolean ConvPCX( const char *name, byte *buffer, size_t filesize, const char *ext )
{
	rgbdata_t	*pic = FS_LoadImage( va( "#%s.pcx", name ), buffer, filesize );

	if( pic )
	{
		FS_SaveImage( va("%s/%s.%s", gs_gamedir, name, ext ), pic );
		Msg( "%s.pcx\n", name ); // echo to console
		FS_FreeImage( pic );
		return true;
	}
	return false;
}

/*
============
ConvMIP
============
*/
qboolean ConvMIP( const char *name, byte *buffer, size_t filesize, const char *ext )
{
	rgbdata_t *pic = FS_LoadImage( va( "#%s.mip", name ), buffer, filesize );
	
	if( pic )
	{
		FS_SaveImage( va("%s/%s.%s", gs_gamedir, name, ext ), pic );
		Conv_WriteQCScript( name, pic );
		Msg( "%s.mip\n", name ); // echo to console
		FS_FreeImage( pic );
		return true;
	}
	return false;
}

/*
============
ConvLMP
============
*/
qboolean ConvLMP( const char *name, byte *buffer, size_t filesize, const char *ext )
{
	rgbdata_t *pic = FS_LoadImage( va( "#%s.lmp", name ), buffer, filesize );

	if( pic )
	{
		FS_SaveImage(va("%s/%s.%s", gs_gamedir, name, ext ), pic );
		Conv_WriteQCScript( va( "%s.lmp", name ), pic );
		Msg("%s.lmp\n", name ); // echo to console
		FS_FreeImage( pic );
		return true;
	}
	return false;
}

/*
============
ConvFNT
============
*/
qboolean ConvFNT( const char *name, byte *buffer, size_t filesize, const char *ext )
{
	rgbdata_t *pic = FS_LoadImage( va( "#%s.fnt", name ), buffer, filesize );

	if( pic )
	{
		FS_SaveImage(va("%s/%s.%s", gs_gamedir, name, ext ), pic );
		Conv_WriteQCScript( name, pic );
		Msg("%s.fnt\n", name ); // echo to console
		FS_FreeImage( pic );
		return true;
	}
	return false;
}