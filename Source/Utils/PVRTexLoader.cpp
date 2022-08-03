/*
Copyright (C) 2003, 2010 - Wolfire Games
Copyright (C) 2010-2017 - Lugaru contributors (see AUTHORS file)

This file is part of Lugaru.

Lugaru is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

Lugaru is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Lugaru.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "Graphic/gamegl.hpp"
#include "Utils/PVRTexLoader.hpp"
#include "Utils/FileCache.hpp"
#include "Utils/Log.h"
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>

PVRTexLoader::PVRTexLoader()
{
	//--
}

void PVRTexLoader::freeTexture(uint8_t *data){
	if(data != NULL){
		free(data);
	}
}

#define get8(out)  if(fread((void*) out, 1, 1, fd) != 1){ LOG("Failed to read 8 bits (" #out ")"); goto fail; }
#define get32(out) if(fread((void*) out, 1, 4, fd) != 4){ LOG("Failed to read 32 bits (" #out ")"); goto fail; }
#define get64(out) if(fread((void*) out, 1, 8, fd) != 8){ LOG("Failed to read 64 bits (" #out ")"); goto fail; }

static bool read_metadata_block(PVRHeader *header, PVRMetaData *meta, FILE *fd){
	get8(&meta->FourCC[0]);
	get8(&meta->FourCC[1]);
	get8(&meta->FourCC[2]);
	get8(&meta->FourCC[3]);

	get32(&meta->Key);
	get32(&meta->DataSize);

	if(meta->FourCC[0] == 'P' && meta->FourCC[1] == 'V' && meta->FourCC[2] == 'R' && meta->FourCC[3] == 3){
		switch(meta->Key){
			default:
				LOG("Unsupported metadata block type (%d) in PVR file", meta->Key);
				goto fail;
				break;

			case 4: //border
				get32(&header->border[0]);
				get32(&header->border[1]);
				get32(&header->border[2]);
				break;

			case 6: //ChannelType overrides
				memset(header->chtype, 0, 4);
				get8(&header->chtype[0]);
				get8(&header->chtype[1]);
				get8(&header->chtype[2]);
				get8(&header->chtype[3]);

				if(
					header->chtype[1] == header->chtype[0] &&
					header->chtype[2] == header->chtype[0] &&
					header->chtype[3] == header->chtype[0] ){
					header->ChannelType = header->chtype[0];
				}else{
					header->ChannelType = -1;
				}

				break;
		}
	}

	return true;

	fail:
		return false;
}
/**
	Populate header with data from file and leave fd pointing
	to the start of the texture data
*/
static bool read_header(PVRHeader *header, FILE *fd){
	if(header == NULL){
		fprintf(stderr, "Header storage buffer is null!\n");
		goto fail;
	}
	memset(header, 0, sizeof(PVRHeader));

	get32(&header->Version);

	if(header->Version != 0x03525650){
		LOG("PVR Header is invalid");
		if(header->Version == 0x50565203){
			LOG("\tEndianness doesn't match!");
		}
		goto fail;
	}
	get32(&header->Flags);
	
	get64(&header->PixelFormat);

	get32(&header->ColorSpace);
	get32(&header->ChannelType);
	get32(&header->Height);
	get32(&header->Width);

	if((header->Height & (header->Height - 1)) != 0){
		LOG("Texture size is not a power of 2! Height: %d", header->Height);
		goto fail;
	}

	if((header->Width & (header->Width - 1)) != 0){
		LOG("Texture size is not a power of 2! Width: %d", header->Width);
		goto fail;
	}

	get32(&header->Depth);
	get32(&header->NumSurfaces);
	get32(&header->NumFaces);
	get32(&header->MipMapCount);
	get32(&header->MetaDataSize);

	if(header->MetaDataSize > 0){
		header->metadata = (PVRMetaData*) malloc(header->MetaDataSize);
		long pos_start = ftell(fd);
		long pos_end;
		int i = 0;
		do {
			if(!read_metadata_block(header, header->metadata + (i++), fd)){
				LOG("Error while reading metadata block at %d", (int)(header->metadata + (i-1)));
				goto fail;
			}
		} while(ftell(fd) - pos_start < header->MetaDataSize);

		pos_end = ftell(fd);
		if(pos_end != pos_start + header->MetaDataSize){
			LOG("Error while reading metadata blocks in PVR file.\n\tpos_start: %d\n\tpos_end: %d\n\tMetaDataSize: %d",
				(int) pos_start, (int) pos_end, (int) header->MetaDataSize
			);
			free(header->metadata);
			header->metadata = NULL;
			goto fail;
		}
		free(header->metadata);
		header->metadata = NULL;
	}

	return true;

	fail:
		return false;
}

static int get_pvr_channel_size(int type){
	switch(type){
		case 0:  return 1; //Unsigned Byte Normalised
		case 1:  return 1; //Signed Byte Normalised
		case 2:  return 1; //Unsigned Byte
		case 3:  return 1; //Signed Byte
		case 4:  return 2; //Unsigned Short Normalised
		case 5:  return 2; //Signed Short Normalised
		case 6:  return 2; //Unsigned Short
		case 7:  return 2; //Signed Short
		case 8:  return 4; //Unsigned Integer Normalised
		case 9:  return 4; //Signed Integer Normalised
		case 10: return 4; //Unsigned Integer
		case 11: return 4; //Signed Integer
		case 12: return 4; //Signed Float
		case 13: return 4; //Unsigned Float
	}
}

bool PVRTexLoader::loadTexture(const char *filename, uint8_t **data, PVRHeader *header){
	//FILE *fd = fopen(filename, "rb");
	FILE *fd = FileCache::readFile(filename);
	if(fd == NULL){
		LOG("Failed to open \"%s\"", filename);
		return false;
	}

	fseek(fd, 0L, SEEK_END);
	size_t filesize = ftell(fd);
	rewind(fd);

	if(!read_header(header, fd)){
		LOG("Failed to read header for \"%s\"", filename);
		return false;
	}

	size_t size = (filesize - (52 + header->MetaDataSize));
	header->img_size = size;

	*data = (uint8_t*) malloc(size);

	if(*data == nullptr){
		LOG("Failed to allocate %d bytes", (int) size);
		fclose(fd);
		return false;
	}

	fread(*data, size, 1, fd);

	if(ferror(fd)){
		LOG("Error(%d) while reading \"%s\"", errno, filename);
		fclose(fd);
		free(*data);
		*data = NULL;
		return false;
	}

	fclose(fd);
	return true;
}
/**
 * In a "Pixel Transfer" operation, the format specifies:
 * 
 * 	- data 'type', such as: color, depth, stencil, or depth/stencil
 *  - component order
 *  - (only for color data) whether to convert values to/from floating point
 * 
 * This returns the type and order for color types only
 * */
static GLuint parse_gl_pixel_format(uint8_t order[4], uint8_t rate[4]){
	//LOG("\tparse_gl_pixel_format()\n\t\torder: %c %c %c %c\n\t\trate: %d %d %d %d",
	//	(char)order[0], (char)order[1], (char)order[2], (char)order[3],
	//	(int)rate[0], (int)rate[1], (int)rate[2], (int)rate[3]
	//);
	if((order[1] + order[2] + order[3]) == 0){
		switch(order[0]){
			case 'r': return GL_RED;
			case 'g': return GL_GREEN;
			case 'b': return GL_BLUE;
			case 'a': return GL_ALPHA;
			case 'l': return 0; //luminance (?)
			case 'i': return 0; //?
			case 'd': return 0; //depth
			case 's': return 0; //stencil
			case 'x': return 0; //?
		}
	}else if(order[0] == 'r' && order[1] == 'g' && order[2] == 'b'){
		if(order[3] == 'a'){
			return GL_RGBA;
		}else if(order[3] == 0){
			return GL_RGB;
		}
	}else if(order[0] == 'b' && order[1] == 'g' && order[2] == 'r'){
		if(order[3] == 'a'){
			return GL_BGRA;
		}else if(order[3] == 0){
			return GL_BGR;
		}
	}
	return 0;
}

GLint PVRHeader::getBorder(int side) const{
	return border[side];
}

GLuint PVRHeader::getGLPixelFormat() const{
	uint8_t order[4], rate[4];
	if(getUncompressedChannelOrder(order, rate)){
		return parse_gl_pixel_format(order, rate);
	}
	return 0;
}

GLenum get_channel_gl_unitype_ratio(uint32_t unitype, uint8_t order[4], uint8_t rate[4]){
	switch(unitype){
		case 0:
		case 2:
			if(rate[0] == 3 && rate[1] == 3 && rate[2] == 2){
				//return GL_UNSIGNED_BYTE_3_3_2;
				return 0; //VITAGL: TODO
			}else if(rate[0] == 2 && rate[1] == 2 && rate[2] == 3){
				//don't use _REV suffix since `parse_gl_pixel_format` already encodes the order
				//return GL_UNSIGNED_BYTE_3_3_2;
				return 0; //VITAGL: TODO
			}else{
				return GL_UNSIGNED_BYTE;
			}

		case 4:
		case 6:
			if(rate[0] == 5 && rate[1] == 6 && rate[2] == 5){
				return GL_UNSIGNED_SHORT_5_6_5;
			}else if(rate[0] == 4 && rate[1] == 4 && rate[2] == 4 && rate[3] == 4){
				return GL_UNSIGNED_SHORT_4_4_4_4;
			}else if(rate[0] == 5 && rate[1] == 5 && rate[2] == 5 && rate[3] == 1){
				return GL_UNSIGNED_SHORT_5_5_5_1;
			} if(rate[0] == 1 && rate[1] == 5 && rate[2] == 5 && rate[3] == 5){
				return GL_UNSIGNED_SHORT_5_5_5_1;
			}else{
				return GL_UNSIGNED_SHORT;
			}

		case 8:
		case 10:
			if(rate[0] == 8 && rate[1] == 8 && rate[2] == 8 && rate[3] == 8){
				//return GL_UNSIGNED_INT_8_8_8_8;
				return 0; //VITAGL: TODO
			}else if(rate[0] == 10 && rate[1] == 10 && rate[2] == 10 && rate[3] == 2){
				//return GL_UNSIGNED_INT_10_10_10_2;
				return 0; //VITAGL: TODO
			}else{
				return GL_UNSIGNED_INT;
			}

		//signed types
		case 12:
		case 13:
		case 9:
		case 11:
		case 5:
		case 7:
		case 1:
		case 3:
			return unitype;

		default: 
			return 0;
	}
}

/**
 * 	Only call this for uncompressed PixelFormats
 * 
 * This returns the pixel type in a pixel transfer operation, but only
 * for uniform-rate pixel formats. If rates are not uniform, pass the
 * output of this function to `get_channel_gl_unitype_ratio`
 * */
GLenum get_channel_gl_unitype(uint32_t ChannelType) {
	//Only works if all channels are the same type
	if(ChannelType != -1){
		switch(ChannelType){
			case 0: //normalized
			case 2:
				return GL_UNSIGNED_BYTE;
			case 1: //normalized
			case 3:
				return GL_BYTE;
			case 4: //normalized
			case 6:
				return GL_UNSIGNED_SHORT;
			case 5: //normalized
			case 7:
				return GL_SHORT;
			case 8: //normalized
			case 10:
				return GL_UNSIGNED_INT;
			case 9: //normalized
			case 11:
				return GL_INT;
			case 12:
			case 13://unsigned(?)
				return GL_FLOAT;
		}
	}else{
		return 0;//Mixed channel types not supported
	}
}

GLsizei PVRHeader::getBitsPerPixel() const{
	uint8_t order[4], rate[4];
	if(getUncompressedChannelOrder(order, rate)){
		GLsizei ret = 0;
		for(int i = 0; i < 4; i++){
			ret += rate[i];
		}
		return ret;
	}else{
		return 0; //TODO
	}
}

GLenum PVRHeader::getGLPixelType() const{
	GLenum ut = get_channel_gl_unitype(ChannelType);
	if(ChannelType != -1){
		return ut;
	}else{
		uint8_t order[4], rate[4];
		getUncompressedChannelOrder(order, rate);
		return get_channel_gl_unitype_ratio(ut, order, rate);
	}
}

GLsizei PVRHeader::getImageSize() const{
	return img_size;
}

bool PVRHeader::isCompressed() const{
	return PixelFormat < (uint64_t) PVRPixelFormat::COMPRESSED_END;
}

bool PVRHeader::getUncompressedChannelOrder(uint8_t order[4], uint8_t rate[4]) const{
	/**
	 * Notes:
	 * 
	 * 	order values are chars specifying the channel type, and these can be:
	 * 		- 'r', 'g', 'b', 'a', 'l', 'i', 'd', 's', 'x', 0
	 * 		- a value of zero means that channel is missing
	 * 
	 *  rate values specify how many bits are allocated to that channel per pixel
	 * 
	 * */
	uint32_t msb = (PixelFormat & 0xFFFFFFFF00000000L) >> 32;
	if(msb){
		if(order != NULL){
			uint32_t lsb = PixelFormat & 0x00000000FFFFFFFFL;
			order[0] = (lsb >> 0) & 0xFF;
			order[1] = (lsb >> 8) & 0xFF;
			order[2] = (lsb >> 16) & 0xFF;
			order[3] = (lsb >> 24) & 0xFF;
		}
		if(rate != NULL){
			rate[0] = (msb >> 0) & 0xFF;
			rate[1] = (msb >> 8) & 0xFF;
			rate[2] = (msb >> 16) & 0xFF;
			rate[3] = (msb >> 24) & 0xFF;
		}
		return true;
	}
	return false;
}
GLuint PVRHeader::getGLInternalFormat() const{
	//TODO: test this
	uint8_t order[4], rate[4];
	if(getUncompressedChannelOrder(order, rate)){
		return parse_gl_pixel_format(order, rate);
	}else{
		switch(PixelFormat){
			case 0: //PVRTC 2bpp RGB 
				return GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG;
			case 1: //PVRTC 2bpp RGBA 
				return GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG;
			case 2: //PVRTC 4bpp RGB 
				return GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG;
			case 3: //PVRTC 4bpp RGBA 
				return GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG;
			case 4: //PVRTC-II 2bpp 
				return GL_COMPRESSED_RGBA_PVRTC_2BPPV2_IMG;
			case 5: //PVRTC-II 4bpp
				return GL_COMPRESSED_RGBA_PVRTC_4BPPV2_IMG;
			case 6: //ETC1
				return GL_ETC1_RGB8_OES;
			case 7: //DXT1
				return GL_COMPRESSED_SRGB_S3TC_DXT1;
			case 9: //DXT3
				return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
			case 11: //DXT5
				return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5;
			default:
				return 0;
		}
	}
}

#define CLAMP_MAX(v, m) ((v) > (m) ? (m) : (v))
#define CLAMP_MIN(v, m) ((v) < (m) ? (m) : (v))

static size_t calc_mipmap_offset(int level, size_t basesz, size_t minsz, size_t padding, size_t alignment) {
	if(level == 0){
		return 0;
	}
	size_t off = 0;
	for(int i = 1; i <= level; i++){
		int d = 2 << (((i-1) * 2) - 1);
		size_t lsz = basesz / (d?d:1);
		off += CLAMP_MIN(lsz, minsz) + padding;
		if(alignment != 0){
			size_t mod = off % alignment;
			if(mod){
				off += alignment - mod;
			}
		}
	}
	ASSERT(!(off % alignment) && "bad alignment");
	return off;
}

bool PVRHeader::getMipMap(int level, PVRMipMapLevel *mipmap) const{
	ASSERT(NumSurfaces == 1 && "Texture arrays are not supported");
	ASSERT(NumFaces == 1 && "Cube maps are not supported");
	ASSERT(Depth == 1 && "3D textures are not supported");
	
	if(level < 0 || level >= MipMapCount){
		return false;
	}

	int base_denom = pow(2, level);
	mipmap->Width = Width / base_denom;
	mipmap->Height = Height / base_denom;
	mipmap->Depth = 1;

	if(!isCompressed()){
		int bytes_per_pixel = getBitsPerPixel() / 8;
		int basesz = Width * Height * bytes_per_pixel;
		mipmap->size = basesz / base_denom;
		mipmap->offset = calc_mipmap_offset(level, basesz, 0, 0, 0);
		return true;
	}else{
		size_t basesz = 0, minsz = 0;
		size_t alignment = 0, padding = 0;

		switch((PVRPixelFormat)PixelFormat){
			default: return false; //not implemented (yet)

			case PVRPixelFormat::PVRTCII_2bpp:
			case PVRPixelFormat::PVRTC_2bpp_RGBA:
			case PVRPixelFormat::PVRTC_2bpp_RGB:
				basesz = (Width * Height * 2) / 8;
				minsz = 8;
				mipmap->size = (CLAMP_MIN(mipmap->Width, 8) * CLAMP_MIN(mipmap->Height, 4) * 2) / 8;
				break;

			case PVRPixelFormat::PVRTCII_4bpp:
			case PVRPixelFormat::PVRTC_4bpp_RGBA:
			case PVRPixelFormat::PVRTC_4bpp_RGB:
				basesz = (Width * Height * 4) / 8;
				minsz = 8;
				mipmap->size = (CLAMP_MIN(mipmap->Width, 4) * CLAMP_MIN(mipmap->Height, 4) * 4) / 8;
		}

		mipmap->size = CLAMP_MIN(mipmap->size, minsz);
		mipmap->offset = calc_mipmap_offset(level, basesz, minsz, padding, alignment);
		return true;
	}
}