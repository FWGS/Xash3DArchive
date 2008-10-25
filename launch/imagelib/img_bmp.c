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
bool Image_LoadBMP( const char *name, const byte *buffer, size_t filesize )
{
	byte	*buf_p, *pb, *pbBmpBits;
	int	i, columns, rows;
	RGBQUAD	rgrgbPalette[256];
	dword	cbBmpBits;
	dword	cbPalBytes;
	dword	biTrueWidth;
	bool	result = false;
	bmp_t	bhdr;

	buf_p = (byte *)buffer;
	bhdr.id[0] = *buf_p++;
	bhdr.id[1] = *buf_p++;				// move pointer
	bhdr.fileSize = LittleLong(*(long *)buf_p);		buf_p += 4;
	bhdr.reserved0 = LittleLong(*(long *)buf_p);		buf_p += 4;
	bhdr.bitmapDataOffset = LittleLong(*(long *)buf_p);	buf_p += 4;
	bhdr.bitmapHeaderSize = LittleLong(*(long *)buf_p);	buf_p += 4;
	bhdr.width = LittleLong(*(long *)buf_p);		buf_p += 4;
	bhdr.height = LittleLong(*(long *)buf_p);		buf_p += 4;
	bhdr.planes = LittleShort(*(short *)buf_p);		buf_p += 2;
	bhdr.bitsPerPixel = LittleShort(*(short *)buf_p);		buf_p += 2;
	bhdr.compression = LittleLong(*(long *)buf_p);		buf_p += 4;
	bhdr.bitmapDataSize = LittleLong(*(long *)buf_p);		buf_p += 4;
	bhdr.hRes = LittleLong(*(long *)buf_p);			buf_p += 4;
	bhdr.vRes = LittleLong(*(long *)buf_p);			buf_p += 4;
	bhdr.colors = LittleLong(*(long *)buf_p);		buf_p += 4;
	bhdr.importantColors = LittleLong(*(long *)buf_p);	buf_p += 4;
	Mem_Copy( bhdr.palette, buf_p, sizeof( bhdr.palette ));
	
	// bogus file header check
	if( bhdr.reserved0 != 0 ) return false;

	if( memcmp(bhdr.id, "BM", 2 ))
	{
		MsgDev( D_ERROR, "Image_LoadBMP: only Windows-style BMP files supported (%s)\n", name );
		return false;
	} 

	// bogus info header check
	if( bhdr.fileSize != filesize )
	{
		MsgDev( D_ERROR, "Image_LoadBMP: incorrect file size %i should be %i\n", filesize, bhdr.fileSize );
		return false;
          }
          
	// bogus bit depth?  Only 8-bit supported.
	if( bhdr.bitsPerPixel != 8 )
	{
		MsgDev( D_ERROR, "Image_LoadBMP: %d not a 8 bit image\n", bhdr.bitsPerPixel );
		return false;
	}
	
	// bogus compression?  Only non-compressed supported.
	if( bhdr.compression != BI_RGB ) 
	{
		MsgDev( D_ERROR, "Image_LoadBMP: it's compressed file\n");
		return false;
	}

	image.width = bhdr.width;
	image.height = bhdr.height;
	if(!Image_ValidSize( name ))
		return false;          

	// figure out how many entires are actually in the table
	if( bhdr.colors == 0 )
	{
		bhdr.colors = 256;
		cbPalBytes = (1 << bhdr.bitsPerPixel) * sizeof( RGBQUAD );
	}
	else cbPalBytes = bhdr.colors * sizeof( RGBQUAD );
	Mem_Copy( rgrgbPalette, &bhdr.palette, cbPalBytes ); // read palette (bmih.biClrUsed entries)

	// convert to a unpacked 1024 byte palette
	pb = image.palette = Mem_Alloc( Sys.imagepool, 1024 );

	// copy over used entries
	for( i = 0; i < (int)bhdr.colors; i++ )
	{
		*pb++ = rgrgbPalette[i].rgbRed;
		*pb++ = rgrgbPalette[i].rgbGreen;
		*pb++ = rgrgbPalette[i].rgbBlue;
		*pb++ = rgrgbPalette[i].rgbReserved;
	}

	// fill in unused entires will 0, 0, 0
	for( i = bhdr.colors; i < 256; i++ ) 
	{
		*pb++ = 0;
		*pb++ = 0;
		*pb++ = 0;
		*pb++ = 0;
	}

	// read bitmap bits (remainder of file)
	columns = bhdr.width, rows = bhdr.height;
	if ( rows < 0 ) rows = -rows;
	cbBmpBits = columns * rows;
          buf_p += 1024; // move pointer
          
	pb = buf_p;
	pbBmpBits = Mem_Alloc( Sys.imagepool, cbBmpBits );

	// data is actually stored with the width being rounded up to a multiple of 4
	biTrueWidth = (bhdr.width + 3) & ~3;
	
	// reverse the order of the data.
	pb += (bhdr.height - 1) * biTrueWidth;
	for( i = 0; i < bhdr.height; i++ )
	{
		Mem_Copy( &pbBmpBits[biTrueWidth * i], pb, biTrueWidth );
		pb -= biTrueWidth;
	}

	pb += biTrueWidth;
	image.num_layers = image.num_mips = 1;
	image.type = PF_INDEXED_32; // 32 bit palette

	// scan for transparency
	for( i = 0; i < image.width * image.height; i++ )
	{
		if( pbBmpBits[i] == 255 )
		{
			image.flags |= IMAGE_HAVE_ALPHA;
			break;
		}
	}

	result = FS_AddMipmapToPack( pbBmpBits, image.width, image.height );
	Mem_Free( pbBmpBits );
	return result;
}

bool Image_SaveBMP( const char *name, rgbdata_t *pix )
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

	if(FS_FileExists( name ) && !(image.cmd_flags & IL_ALLOW_OVERWRITE ))
		return false; // already existed

	// bogus parameter check
	if( !pix->palette || !pix->buffer )
		return false;

	pfile = FS_Open( name, "wb");
	if(!pfile) return false;

	switch( pix->type )
	{
	case PF_INDEXED_24:
	case PF_INDEXED_32:
		break;
	default:
		MsgDev( D_WARN, "Image_SaveBMP: unsupported image type %s\n", PFDesc[pix->type].name );
		return false;
	}

	// NOTE: alig transparency column will sucessfully removed
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

	// make last color is 0 0 255, xwad expect this
	if( pix->flags & IMAGE_HAVE_ALPHA )
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
	memset( pbBmpBits, 0xFF, cbBmpBits );	// fill buffer with black color

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