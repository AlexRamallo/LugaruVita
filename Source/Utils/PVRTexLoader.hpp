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
#ifndef __PVR_TEX_LOADER___
#define __PVR_TEX_LOADER___
#include <string>

/**
	https://docs.imgtec.com/specifications/pvr-file-format-specification/topics/pvr-metadata.html

	Simple loader that supports only the features we need
*/
struct PVRMetaData {
	uint8_t FourCC[4];
	uint32_t Key;
	uint32_t DataSize;
	uint8_t *Data;
};

struct PVRHeader {
	uint32_t Version;
	uint32_t Flags;
	uint64_t PixelFormat;
	uint32_t ColorSpace;
	uint32_t ChannelType;
	uint32_t Height;
	uint32_t Width;
	uint32_t Depth;
	uint32_t NumSurfaces;
	uint32_t NumFaces;
	uint32_t MipMapCount;
	uint32_t MetaDataSize;

	PVRMetaData *metadata;

	bool isCompressed();
	GLuint getGLInternalFormat();
	GLsizei getImageSize();	
	GLint getBorder(int side = 0);
	//only if not compressed
	GLuint getGLPixelFormat();

	//there aren't part of the standard PVR header
	GLsizei img_size;
	GLint border[3];
private:
	bool getUncompressedChannelOrder(uint8_t order[4], uint8_t rate[4]);
};

class PVRTexLoader {
public:
	PVRTexLoader();
	~PVRTexLoader();

	bool loadTexture(const char *filename, uint8_t **data, PVRHeader *header);
	void freeTexture(uint8_t *data);
};

#endif //__PVR_TEX_LOADER___