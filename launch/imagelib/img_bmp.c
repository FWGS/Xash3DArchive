//=======================================================================
//			Copyright XashXT Group 2007 ©
//			img_bmp.c - bmp format load & save
//=======================================================================

#include "imagelib.h"

/*
=============
Image_LoadBMP
=============
*/
qboolean Image_LoadBMP( const char *name, const byte *buffer, size_t filesize )
{
	byte	*buf_p, *pixbuf;
	byte	palette[256][4];
	int	i, columns, column, rows, row, bpp = 1;
	int	cbPalBytes = 0, padSize = 0, bps = 0;
	bmp_t	bhdr;

	if( filesize < sizeof( bhdr )) return false; 

	buf_p = (byte *)buffer;
	bhdr.id[0] = *buf_p++;
	bhdr.id[1] = *buf_p++;				// move pointer
	bhdr.fileSize = *(long *)buf_p;	buf_p += 4;
	bhdr.reserved0 = *(long *)buf_p;	buf_p += 4;
	bhdr.bitmapDataOffset = *(long *)buf_p;	buf_p += 4;
	bhdr.bitmapHeaderSize = *(long *)buf_p;	buf_p += 4;
	bhdr.width = *(long *)buf_p;		buf_p += 4;
	bhdr.height = *(long *)buf_p;		buf_p += 4;
	bhdr.planes = *(short *)buf_p;	buf_p += 2;
	bhdr.bitsPerPixel = *(short *)buf_p;	buf_p += 2;
	bhdr.compression = *(long *)buf_p;	buf_p += 4;
	bhdr.bitmapDataSize = *(long *)buf_p;	buf_p += 4;
	bhdr.hRes = *(long *)buf_p;		buf_p += 4;
	bhdr.vRes = *(long *)buf_p;		buf_p += 4;
	bhdr.colors = *(long *)buf_p;		buf_p += 4;
	bhdr.importantColors = *(long *)buf_p;	buf_p += 4;

	// bogus file header check
	if( bhdr.reserved0 != 0 ) return false;
	if( bhdr.planes != 1 ) return false;

	if( memcmp( bhdr.id, "BM", 2 ))
	{
		MsgDev( D_ERROR, "Image_LoadBMP: only Windows-style BMP files supported (%s)\n", name );
		return false;
	} 

	if( bhdr.bitmapHeaderSize != 0x28 )
	{
		MsgDev( D_ERROR, "Image_LoadBMP: invalid header size %i\n", bhdr.bitmapHeaderSize );
		return false;
	}

	// bogus info header check
	if( bhdr.fileSize != filesize )
	{
		MsgDev( D_ERROR, "Image_LoadBMP: incorrect file size %i should be %i\n", filesize, bhdr.fileSize );
		return false;
          }
          
	// bogus compression?  Only non-compressed supported.
	if( bhdr.compression != BI_RGB ) 
	{
		MsgDev( D_ERROR, "Image_LoadBMP: only uncompressed BMP files supported (%s)\n", name );
		return false;
	}

	image.width = columns = bhdr.width;
	image.height = rows = abs( bhdr.height );

	if(!Image_ValidSize( name ))
		return false;          

	if( bhdr.bitsPerPixel <= 8 )
	{
		// figure out how many entires are actually in the table
		if( bhdr.colors == 0 )
		{
			bhdr.colors = 256;
			cbPalBytes = (1 << bhdr.bitsPerPixel) * sizeof( RGBQUAD );
		}
		else cbPalBytes = bhdr.colors * sizeof( RGBQUAD );
	}

	Mem_Copy( palette, buf_p, cbPalBytes );

	if( image.cmd_flags & IL_KEEP_8BIT && bhdr.bitsPerPixel == 8 )
	{
		pixbuf = image.palette = Mem_Alloc( Sys.imagepool, 1024 );
		image.flags |= IMAGE_HAS_COLOR;
 
		// bmp have a reversed palette colors
		for( i = 0; i < bhdr.colors; i++ )
		{
			*pixbuf++ = palette[i][2];
			*pixbuf++ = palette[i][1];
			*pixbuf++ = palette[i][0];
			*pixbuf++ = palette[i][3];
		}
		image.type = PF_INDEXED_32; // 32 bit palette
	}
	else
	{
		image.palette = NULL;
		image.type = PF_RGBA_32;
		bpp = 4;
	}

	buf_p += cbPalBytes;
	image.size = image.width * image.height * bpp;
	image.rgba = Mem_Alloc( Sys.imagepool, image.size );
	bps = image.width * (bhdr.bitsPerPixel >> 3);

	switch( bhdr.bitsPerPixel )
	{
	case 1:
		padSize = (( 32 - ( bhdr.width % 32 )) / 8 ) % 4;
		break;
	case 4:
		padSize = (( 8 - ( bhdr.width % 8 )) / 2 ) % 4;
		break;
	case 16:
		padSize = ( 4 - ( image.width * 2 % 4 )) % 4;
		break;
	case 8:
	case 24:
		padSize = ( 4 - ( bps % 4 )) % 4;
		break;
	}

	for( row = rows - 1; row >= 0; row-- )
	{
		pixbuf = image.rgba + (row * columns * bpp);

		for( column = 0; column < columns; column++ )
		{
			byte	red, green, blue, alpha;
			word	shortPixel;
			int	c, k, palIndex;

			switch( bhdr.bitsPerPixel )
			{
			case 1:
				alpha = *buf_p++;
				column--;	// ingnore main iterations
				for( c = 0, k = 128; c < 8; c++, k >>= 1 )
				{
					red = (!!(alpha & k) == 1 ? 0xFF : 0x00);
					*pixbuf++ = red;
					*pixbuf++ = red;
					*pixbuf++ = red;
					*pixbuf++ = 0x00;
					if( ++column == columns )
						break;
				}
				break;
			case 4:
				alpha = *buf_p++;
				palIndex = alpha >> 4;
				*pixbuf++ = palette[palIndex][2];
				*pixbuf++ = palette[palIndex][1];
				*pixbuf++ = palette[palIndex][0];
				*pixbuf++ = palette[palIndex][3];
				if( ++column == columns ) break;
				palIndex = alpha & 0x0F;
				*pixbuf++ = palette[palIndex][2];
				*pixbuf++ = palette[palIndex][1];
				*pixbuf++ = palette[palIndex][0];
				*pixbuf++ = palette[palIndex][3];
				break;
			case 8:
				palIndex = *buf_p++;
				red = palette[palIndex][2];
				green = palette[palIndex][1];
				blue = palette[palIndex][0];
				alpha = palette[palIndex][3];

				if( image.cmd_flags & IL_KEEP_8BIT )
				{
					*pixbuf++ = palIndex;
				}
				else
				{
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = alpha;
				}
				break;
			case 16:
				shortPixel = *(word *)buf_p, buf_p += 2;
				*pixbuf++ = blue = (shortPixel & ( 31 << 10 )) >> 7;
				*pixbuf++ = green = (shortPixel & ( 31 << 5 )) >> 2;
				*pixbuf++ = red = (shortPixel & ( 31 )) << 3;
				*pixbuf++ = 0xff;
				break;
			case 24:
				blue = *buf_p++;
				green = *buf_p++;
				red = *buf_p++;
				*pixbuf++ = red;
				*pixbuf++ = green;
				*pixbuf++ = blue;
				*pixbuf++ = 0xFF;
				break;
			case 32:
				blue = *buf_p++;
				green = *buf_p++;
				red = *buf_p++;
				alpha = *buf_p++;
				*pixbuf++ = red;
				*pixbuf++ = green;
				*pixbuf++ = blue;
				*pixbuf++ = alpha;
				if( alpha != 255 ) image.flags |= IMAGE_HAS_ALPHA;
				break;
			default:
				MsgDev( D_ERROR, "Image_LoadBMP: illegal pixel_size (%s)\n", name );
				Mem_Free( image.palette );
				Mem_Free( image.rgba );
				return false;
			}
			if(!( image.cmd_flags & IL_KEEP_8BIT ) && ( red != green || green != blue ))
				image.flags |= IMAGE_HAS_COLOR;
		}
		buf_p += padSize;	// actual only for 4-bit bmps
	}

	if( image.palette ) Image_GetPaletteBMP( image.palette );

	return true;
}

qboolean Image_SaveBMP( const char *name, rgbdata_t *pix )
{
	file_t		*pfile = NULL;
	BITMAPFILEHEADER	bmfh;
	BITMAPINFOHEADER	bmih;
	RGBQUAD		rgrgbPalette[256];
	dword		cbBmpBits;
	byte*		pbBmpBits;
	byte		*pb, *pbPal = NULL;
	dword		cbPalBytes;
	dword		biTrueWidth;
	int		i, rc = 0;

	if( FS_FileExists( name, false ) && !(image.cmd_flags & IL_ALLOW_OVERWRITE ))
		return false; // already existed

	// bogus parameter check
	if( !pix->palette || !pix->buffer )
		return false;

	switch( pix->type )
	{
	case PF_INDEXED_24:
	case PF_INDEXED_32:
		break;
	default:
		MsgDev( D_WARN, "Image_SaveBMP: unsupported image type %s\n", PFDesc[pix->type].name );
		return false;
	}

	pfile = FS_Open( name, "wb", false );
	if( !pfile ) return false;

	// NOTE: align transparency column will sucessfully removed
	// after create sprite or lump image, it's just standard requiriments 
	biTrueWidth = ((pix->width + 3) & ~3);
	cbBmpBits = biTrueWidth * pix->height;
	cbPalBytes = 256 * sizeof( RGBQUAD );

	// Bogus file header check
	bmfh.bfType = MAKEWORD( 'B', 'M' );
	bmfh.bfSize = sizeof(bmfh) + sizeof(bmih) + cbBmpBits + cbPalBytes;
	bmfh.bfReserved1 = 0;
	bmfh.bfReserved2 = 0;
	bmfh.bfOffBits = sizeof(bmfh) + sizeof(bmih) + cbPalBytes;

	// write header
	FS_Write( pfile, &bmfh, sizeof(bmfh));

	// size of structure
	bmih.biSize = sizeof(bmih);
	bmih.biWidth = biTrueWidth;
	bmih.biHeight = pix->height;
	bmih.biPlanes = 1;
	bmih.biBitCount = 8;
	bmih.biCompression = BI_RGB;
	bmih.biSizeImage = 0;
	bmih.biXPelsPerMeter = 0;
	bmih.biYPelsPerMeter = 0;
	bmih.biClrUsed = 256;
	bmih.biClrImportant = 0;
	
	// Write info header
	FS_Write( pfile, &bmih, sizeof(bmih));
	pb = pix->palette;

	// copy over used entries
	for( i = 0; i < (int)bmih.biClrUsed; i++ )
	{
		rgrgbPalette[i].rgbRed = *pb++;
		rgrgbPalette[i].rgbGreen = *pb++;
		rgrgbPalette[i].rgbBlue = *pb++;

		// bmp feature - can store 32-bit palette if present
		// some viewers e.g. fimg.exe can show alpha-chanell for it
		if( pix->type == PF_INDEXED_32 )
			rgrgbPalette[i].rgbReserved = *pb++;
		else rgrgbPalette[i].rgbReserved = 0;
	}

	// make last color is 0 0 255, xwad expect this (but ignore decals)
	if( com.strrchr( name, '{' ) && pix->flags & IMAGE_HAS_ALPHA && !( pix->flags & IMAGE_COLORINDEX ))
	{
		rgrgbPalette[255].rgbRed = 0x00;
		rgrgbPalette[255].rgbGreen = 0x00;
		rgrgbPalette[255].rgbBlue = 0xFF;
		rgrgbPalette[255].rgbReserved = 0x00;
	}

	// write palette( bmih.biClrUsed entries )
	cbPalBytes = bmih.biClrUsed * sizeof( RGBQUAD );
	FS_Write( pfile, rgrgbPalette, cbPalBytes );
	pbBmpBits = Mem_Alloc( Sys.imagepool, cbBmpBits );
	Mem_Set( pbBmpBits, 0xFF, cbBmpBits );	// fill buffer with last palette color

	pb = pix->buffer;
	pb += (pix->height - 1) * pix->width;

	for( i = 0; i < bmih.biHeight; i++ )
	{
		Mem_Copy( &pbBmpBits[biTrueWidth * i], pb, pix->width );
		pb -= pix->width;
	}

	// write bitmap bits (remainder of file)
	FS_Write( pfile, pbBmpBits, cbBmpBits );
	FS_Close( pfile );
	Mem_Free( pbBmpBits );

	return true;
}