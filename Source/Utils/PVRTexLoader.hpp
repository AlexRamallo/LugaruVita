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

struct PVRMipMapLevel {
	//offset from start of image data (bytes)
	size_t offset;
	//size of level data (bytes)
	size_t size;
	size_t Width;
	size_t Height;
	size_t Depth;
};

enum class PVRPixelFormat: uint64_t {
	PVRTC_2bpp_RGB				= 0,
	PVRTC_2bpp_RGBA				= 1,
	PVRTC_4bpp_RGB				= 2,
	PVRTC_4bpp_RGBA				= 3,
	PVRTCII_2bpp				= 4,
	PVRTCII_4bpp				= 5,
	ETC1						= 6,
	DXT1						= 7,
	DXT2						= 8,
	DXT3						= 9,
	DXT4						= 10,
	DXT5						= 11,
	BC1							= 7,
	BC2							= 9,
	BC3							= 11,
	BC4							= 12,
	BC5							= 13,
	BC6							= 14,
	BC7							= 15,
	UYVY						= 16,
	YUY2						= 17,
	BW1bpp						= 18,
	R9G9B9E5_Shared_Exponent	= 19,
	RGBG8888					= 20,
	GRGB8888					= 21,
	ETC2_RGB					= 22,
	ETC2_RGBA					= 23,
	ETC2_RGB_A1					= 24,
	EAC_R11						= 25,
	EAC_RG11					= 26,
	ASTC_4x4					= 27,
	ASTC_5x4					= 28,
	ASTC_5x5					= 29,
	ASTC_6x5					= 30,
	ASTC_6x6					= 31,
	ASTC_8x5					= 32,
	ASTC_8x6					= 33,
	ASTC_8x8					= 34,
	ASTC_10x5					= 35,
	ASTC_10x6					= 36,
	ASTC_10x8					= 37,
	ASTC_10x10					= 38,
	ASTC_12x10					= 39,
	ASTC_12x12					= 40,
	ASTC_3x3x3					= 41,
	ASTC_4x3x3					= 42,
	ASTC_4x4x3					= 43,
	ASTC_4x4x4					= 44,
	ASTC_5x4x4					= 45,
	ASTC_5x5x4					= 46,
	ASTC_5x5x5					= 47,
	ASTC_6x5x5					= 48,
	ASTC_6x6x5					= 49,
	ASTC_6x6x6					= 50,
	BASISU_ETC1S				= 51,
	BASISU_UASTC				= 52,
	RGBM						= 53,
	RGBD						= 54,
	COMPRESSED_END				= 0x00000001FFFFFFFF
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

	/**
	 * Returns a pointer to the start of a mipmap level in the
	 * chain (if available) and the size of that level in bytes.
	 * 
	 * Returns true if successful, false if level is out of range
	 * or texture format is not supported
	 * */
	bool getMipMap(int level, PVRMipMapLevel *mipmap);

	bool isCompressed();
	GLuint getGLInternalFormat();
	GLsizei getImageSize();	
	GLint getBorder(int side = 0);
	
	//only if not compressed
	GLuint getGLPixelFormat();
	GLenum getGLPixelType();

	//returns number of bits per pixel
	GLsizei getBitsPerPixel();

	//there aren't part of the standard PVR header
	GLsizei img_size;
	GLint border[3];
	GLbyte chtype[4]; //overrides
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