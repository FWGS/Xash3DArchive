//=======================================================================
//			Copyright XashXT Group 2007 �
//			img_tga.c - tga format load & save
//=======================================================================

#include "imagelib.h"

/*
=============
Image_LoadTGA
=============
*/
qboolean Image_LoadTGA( const char *name, const byte *buffer, size_t filesize )
{
	int	i, columns, rows, row_inc, row, col;
	byte	*buf_p, *pixbuf, *targa_rgba;
	byte	palette[256][4], red = 0, green = 0, blue = 0, alpha = 0;
	int	readpixelcount, pixelcount;
	qboolean	compressed;
	tga_t	targa_header;

	if( filesize < sizeof( tga_t ))
		return false;

	buf_p = (byte *)buffer;
	targa_header.id_length = *buf_p++;
	targa_header.colormap_type = *buf_p++;
	targa_header.image_type = *buf_p++;

	targa_header.colormap_index = buf_p[0] + buf_p[1] * 256;		buf_p += 2;
	targa_header.colormap_length = buf_p[0] + buf_p[1] * 256;		buf_p += 2;
	targa_header.colormap_size = *buf_p;				buf_p += 1;
	targa_header.x_origin = *(short *)buf_p;			buf_p += 2;
	targa_header.y_origin = *(short *)buf_p;			buf_p += 2;
	targa_header.width = image.width = *(short *)buf_p;		buf_p += 2;
	targa_header.height = image.height = *(short *)buf_p;		buf_p += 2;
	targa_header.pixel_size = *buf_p++;
	targa_header.attributes = *buf_p++;
	if( targa_header.id_length != 0 ) buf_p += targa_header.id_length;	// skip TARGA image comment

	// check for tga file
	if(!Image_ValidSize( name )) return false;

	image.type = PF_RGBA_32; // always exctracted to 32-bit buffer

	if( targa_header.image_type == 1 || targa_header.image_type == 9 )
	{
		// uncompressed colormapped image
		if( targa_header.pixel_size != 8 )
		{
			MsgDev( D_WARN, "Image_LoadTGA: (%s) Only 8 bit images supported for type 1 and 9\n", name );
			return false;
		}
		if( targa_header.colormap_length != 256 )
		{
			MsgDev( D_WARN, "Image_LoadTGA: (%s) Only 8 bit colormaps are supported for type 1 and 9\n", name );
			return false;
		}
		if( targa_header.colormap_index )
		{
			MsgDev( D_WARN, "Image_LoadTGA: (%s) colormap_index is not supported for type 1 and 9\n", name );
			return false;
		}
		if( targa_header.colormap_size == 24 )
		{
			for( i = 0; i < targa_header.colormap_length; i++ )
			{
				palette[i][2] = *buf_p++;
				palette[i][1] = *buf_p++;
				palette[i][0] = *buf_p++;
				palette[i][3] = 255;
			}
		}
		else if( targa_header.colormap_size == 32 )
		{
			for( i = 0; i < targa_header.colormap_length; i++ )
			{
				palette[i][2] = *buf_p++;
				palette[i][1] = *buf_p++;
				palette[i][0] = *buf_p++;
				palette[i][3] = *buf_p++;
			}
		}
		else
		{
			MsgDev( D_WARN, "Image_LoadTGA: (%s) only 24 and 32 bit colormaps are supported for type 1 and 9\n", name );
			return false;
		}
	}
	else if( targa_header.image_type == 2 || targa_header.image_type == 10 )
	{
		// uncompressed or RLE compressed RGB
		if( targa_header.pixel_size != 32 && targa_header.pixel_size != 24 )
		{
			MsgDev( D_WARN, "Image_LoadTGA: (%s) Only 32 or 24 bit images supported for type 2 and 10\n", name );
			return false;
		}
	}
	else if( targa_header.image_type == 3 || targa_header.image_type == 11 )
	{
		// uncompressed greyscale
		if( targa_header.pixel_size != 8 )
		{
			MsgDev( D_WARN, "Image_LoadTGA: (%s) Only 8 bit images supported for type 3 and 11\n", name );
			return false;
		}
	}

	columns = targa_header.width;
	rows = targa_header.height;

	image.size = image.width * image.height * 4;
	targa_rgba = image.rgba = Mem_Alloc( Sys.imagepool, image.size );

	// if bit 5 of attributes isn't set, the image has been stored from bottom to top
	if( targa_header.attributes & 0x20 )
	{
		pixbuf = targa_rgba;
		row_inc = 0;
	}
	else
	{
		pixbuf = targa_rgba + ( rows - 1 ) * columns * 4;
		row_inc = -columns * 4 * 2;
	}

	compressed = ( targa_header.image_type == 9 || targa_header.image_type == 10 || targa_header.image_type == 11 );
	for( row = col = 0; row < rows; )
	{
		pixelcount = 0x10000;
		readpixelcount = 0x10000;

		if( compressed )
		{
			pixelcount = *buf_p++;
			if( pixelcount & 0x80 )  // run-length packet
				readpixelcount = 1;
			pixelcount = 1 + ( pixelcount & 0x7f );
		}

		while( pixelcount-- && ( row < rows ) )
		{
			if( readpixelcount-- > 0 )
			{
				switch( targa_header.image_type )
				{
				case 1:
				case 9:
					// colormapped image
					blue = *buf_p++;
					red = palette[blue][0];
					green = palette[blue][1];
					alpha = palette[blue][3];
					blue = palette[blue][2];
					if( alpha != 255 ) image.flags |= IMAGE_HAS_ALPHA;
					break;
				case 2:
				case 10:
					// 24 or 32 bit image
					blue = *buf_p++;
					green = *buf_p++;
					red = *buf_p++;
					alpha = 255;
					if( targa_header.pixel_size == 32 )
					{
						alpha = *buf_p++;
						if( alpha != 255 )
							image.flags |= IMAGE_HAS_ALPHA;
					}
					break;
				case 3:
				case 11:
					// greyscale image
					blue = green = red = *buf_p++;
					alpha = 255;
					break;
				}
			}

			if( red != green || green != blue )
				image.flags |= IMAGE_HAS_COLOR;

			*pixbuf++ = red;
			*pixbuf++ = green;
			*pixbuf++ = blue;
			*pixbuf++ = alpha;
			if( ++col == columns )
			{
				// run spans across rows
				row++;
				col = 0;
				pixbuf += row_inc;
			}
		}
	}
	return true;
}

/*
=============
Image_SaveTGA
=============
*/
qboolean Image_SaveTGA( const char *name, rgbdata_t *pix )
{
	int		y, outsize, pixel_size;
	const byte	*bufend, *in;
	byte		*buffer, *out;
	const char	*comment = "Generated by Xash ImageLib\0";

	if( FS_FileExists( name, false ) && !( image.cmd_flags & IL_ALLOW_OVERWRITE ))
		return false; // already existed

	if( pix->flags & IMAGE_HAS_ALPHA )
		outsize = pix->width * pix->height * 4 + 18 + com.strlen( comment );
	else outsize = pix->width * pix->height * 3 + 18 + com.strlen( comment );

	buffer = (byte *)Mem_Alloc( Sys.imagepool, outsize );
	Mem_Set( buffer, 0, 18 );

	// prepare header
	buffer[0] = com.strlen( comment ); // tga comment length
	buffer[2] = 2; // uncompressed type
	buffer[12] = (pix->width >> 0) & 0xFF;
	buffer[13] = (pix->width >> 8) & 0xFF;
	buffer[14] = (pix->height >> 0) & 0xFF;
	buffer[15] = (pix->height >> 8) & 0xFF;
	buffer[16] = ( pix->flags & IMAGE_HAS_ALPHA ) ? 32 : 24;
	buffer[17] = ( pix->flags & IMAGE_HAS_ALPHA ) ? 8 : 0; // 8 bits of alpha
	com.strncpy( buffer + 18, comment, com.strlen( comment )); 
	out = buffer + 18 + com.strlen( comment );

	// get image description
	switch( pix->type )
	{
	case PF_RGB_24:
	case PF_BGR_24: pixel_size = 3; break;
	case PF_RGBA_32:
	case PF_BGRA_32: pixel_size = 4; break;	
	default:
		MsgDev( D_ERROR, "Image_SaveTGA: unsupported image type %s\n", PFDesc[pix->type].name );
		Mem_Free( buffer );
		return false;
	}

	switch( pix->type )
	{
	case PF_RGB_24:
	case PF_RGBA_32:
		// swap rgba to bgra and flip upside down
		for( y = pix->height - 1; y >= 0; y-- )
		{
			in = pix->buffer + y * pix->width * pixel_size;
			bufend = in + pix->width * pixel_size;
			for( ; in < bufend; in += pixel_size )
			{
				*out++ = in[2];
				*out++ = in[1];
				*out++ = in[0];
				if( pix->flags & IMAGE_HAS_ALPHA )
					*out++ = in[3];
			}
		}
		break;
	case PF_BGR_24:
	case PF_BGRA_32:
		// flip upside down
		for( y = pix->height - 1; y >= 0; y-- )
		{
			in = pix->buffer + y * pix->width * pixel_size;
			bufend = in + pix->width * pixel_size;
			for( ; in < bufend; in += pixel_size )
			{
				*out++ = in[0];
				*out++ = in[1];
				*out++ = in[2];
				if( pix->flags & IMAGE_HAS_ALPHA )
					*out++ = in[3];
			}
		}
		break;
	}	
	FS_WriteFile( name, buffer, outsize );

	Mem_Free( buffer );
	return true;
}