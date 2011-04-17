//=======================================================================
//			Copyright XashXT Group 2007 �
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

	Q_memcpy( palette, buf_p, cbPalBytes );

	if( image.cmd_flags & IL_KEEP_8BIT && bhdr.bitsPerPixel == 8 )
	{
		pixbuf = image.palette = Mem_Alloc( host.imagepool, 1024 );
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
	image.rgba = Mem_Alloc( host.imagepool, image.size );
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
	dword		cbBmpBits;
	byte		*pb, *pbBmpBits;
	dword		biTrueWidth;
	int		pixel_size;
	int		i, x, y;

	if( FS_FileExists( name, false ) && !(image.cmd_flags & IL_ALLOW_OVERWRITE ))
		return false; // already existed

	// bogus parameter check
	if( !pix->buffer )
		return false;

	// get image description
	switch( pix->type )
	{
	case PF_RGB_24:
		pixel_size = 3;
		break;
	case PF_RGBA_32:
		pixel_size = 4;
		break;	
	default:
		MsgDev( D_ERROR, "Image_SaveBMP: unsupported image type %s\n", PFDesc[pix->type].name );
		return false;
	}

	pfile = FS_Open( name, "wb", false );
	if( !pfile ) return false;

	// NOTE: align transparency column will sucessfully removed
	// after create sprite or lump image, it's just standard requiriments 
	biTrueWidth = ((pix->width + 3) & ~3);
	cbBmpBits = biTrueWidth * pix->height * pixel_size;

	// Bogus file header check
	bmfh.bfType = MAKEWORD( 'B', 'M' );
	bmfh.bfSize = sizeof(bmfh) + sizeof(bmih) + cbBmpBits;
	bmfh.bfReserved1 = 0;
	bmfh.bfReserved2 = 0;
	bmfh.bfOffBits = sizeof(bmfh) + sizeof(bmih);

	// write header
	FS_Write( pfile, &bmfh, sizeof(bmfh));

	// size of structure
	bmih.biSize = sizeof(bmih);
	bmih.biWidth = biTrueWidth;
	bmih.biHeight = pix->height;
	bmih.biPlanes = 1;
	bmih.biBitCount = (pixel_size == 3) ? 24 : 32;
	bmih.biCompression = BI_RGB;
	bmih.biSizeImage = cbBmpBits;
	bmih.biXPelsPerMeter = 0;
	bmih.biYPelsPerMeter = 0;
	bmih.biClrUsed = 0;
	bmih.biClrImportant = 0;
	
	// Write info header
	FS_Write( pfile, &bmih, sizeof(bmih));

	pbBmpBits = Mem_Alloc( host.imagepool, cbBmpBits );

	pb = pix->buffer;

	for( y = 0; y < bmih.biHeight; y++ )
	{
		i = (bmih.biHeight - 1 - y ) * (bmih.biWidth);

		for( x = 0; x < bmih.biWidth; x++ )
		{
			pbBmpBits[i*pixel_size+0] = pb[x*pixel_size+2];
			pbBmpBits[i*pixel_size+1] = pb[x*pixel_size+1];
			pbBmpBits[i*pixel_size+2] = pb[x*pixel_size+0];

			if( pixel_size == 4 ) // write alpha channel
				pbBmpBits[i*pixel_size+3] = pb[x*pixel_size+3];
			i++;
		}
		pb += bmih.biWidth * pixel_size;
	}

	// write bitmap bits (remainder of file)
	FS_Write( pfile, pbBmpBits, cbBmpBits );
	FS_Close( pfile );
	Mem_Free( pbBmpBits );

	return true;
}