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
				uint8_t ch[4];
				get8(&ch[0]);
				get8(&ch[1]);
				get8(&ch[2]);
				get8(&ch[3]);
				//fail if there are any actual overrides
				for(int i = 0; i < 4; i++){
					uint32_t chi = (uint32_t) ch[i];
					if(chi != header->ChannelType){
						LOG("ChannelType overrides aren't supported\n\tHeader: %d\n\tch: %d, %d, %d, %d",
							(int) header->ChannelType,
							(int) ch[0], (int) ch[1], (int) ch[2], (int) ch[3]
						);
						goto fail;
					}
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
	if(header->PixelFormat > 5){
		LOG("Unsupported pixel format: %d", header->PixelFormat);
		goto fail;
	}

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

bool PVRTexLoader::loadTexture(const char *filename, uint8_t **data, PVRHeader *header){
	FILE *fd = fopen(filename, "rb");
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

static GLuint parse_gl_pixel_format(uint8_t order[4], uint8_t rate[4]){
	//no GL_LUMINANCE or GL_LUMINANCE_ALPHA
	if(order[1] == order[2] == order[3] == 0){
		switch(order[0]){
			case 'r': return GL_RED;
			case 'g': return GL_GREEN;
			case 'b': return GL_BLUE;
			case 'a': return GL_ALPHA;
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

GLint PVRHeader::getBorder(int side) {
	return border[side];
}

GLuint PVRHeader::getGLPixelFormat(){
	uint8_t order[4], rate[4];
	if(getUncompressedChannelOrder(order, rate)){
		return parse_gl_pixel_format(order, rate);
	}else{
		switch(PixelFormat){
			default:
				return 0;
			case 0:
			case 2:
				return GL_RGB;
			case 1:
			case 3:
			case 4:
			case 5:
				return GL_RGBA;
		}
	}
}

GLsizei PVRHeader::getImageSize(){
	return img_size;
}

bool PVRHeader::isCompressed(){
	uint8_t order[4], rate[4];
	return !getUncompressedChannelOrder(order, rate);
}

bool PVRHeader::getUncompressedChannelOrder(uint8_t order[4], uint8_t rate[4]){
	//TODO: test this
	uint32_t msb = PixelFormat & 0xFFFFFFFF00000000;
	if(msb){
		if(order != NULL){
			uint32_t lsb = PixelFormat & 0x00000000FFFFFFFF;
			order[0] = (PixelFormat >> 0) & 0xFF;
			order[1] = (PixelFormat >> 1) & 0xFF;
			order[2] = (PixelFormat >> 2) & 0xFF;
			order[3] = (PixelFormat >> 3) & 0xFF;
		}
		if(rate != NULL){
			rate[3] = (PixelFormat << 1) & 0xFF;
			rate[2] = (PixelFormat << 2) & 0xFF;
			rate[1] = (PixelFormat << 3) & 0xFF;
			rate[0] = (PixelFormat << 4) & 0xFF;
		}
		return true;
	}
	return false;
}

GLuint PVRHeader::getGLInternalFormat(){
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