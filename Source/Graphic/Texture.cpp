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

#include "Graphic/Texture.hpp"

#include "Thirdparty/microprofile/microprofile.h"
#include "Utils/Folders.hpp"
#include "Utils/ImageIO.hpp"
#include "Utils/Log.h"
#include <assert.h>

using namespace std;

extern PVRTexLoader *pvr_loader;
extern bool trilinear;

void TextureRes::uploadPVR(void *pTexture){
	MICROPROFILE_SCOPEI("TextureRes", "uploadPVR", 0x008fff);
	ImageRec *texture = (ImageRec*) pTexture;
	ASSERT(texture->is_pvr);

	//datalen = texture->pvr_header.getImageSize();

	glDeleteTextures(1, &id);
	glGenTextures(1, &id);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glBindTexture(GL_TEXTURE_2D, id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	if (hasMipmap) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (trilinear ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_NEAREST));
	} else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
	
	ASSERT(texture->pvr_header.getBorder(0) == texture->pvr_header.getBorder(1) && "PVR texture has uneven borders");
	
	int sizeBorder = texture->pvr_header.getBorder();
	int sizeX = texture->pvr_header.Width + (2 * sizeBorder);
	skinsize = sizeX;

	if (isSkin) {
		MICROPROFILE_SCOPEI("TextureRes", "proc-skin", 0x008fff);
		if(texture->pvr_header.isCompressed()){
			LOG("ERROR: Compressed PVR skin texture found");
			return;
		}

		free(data);
		int sizeY = texture->pvr_header.Height + sizeBorder;
		GLsizei bpp = texture->pvr_header.getBitsPerPixel();

		GLuint type = 0;
		if(bpp == 24){
			type = GL_RGB;
		}else if(bpp == 32){
			type = GL_RGBA;
		}

		const int nb = sizeX * sizeY * (bpp / 8);
		ASSERT(nb == texture->pvr_header.getImageSize());

		ASSERT(type > 0);

		data = (GLubyte*)malloc(nb * sizeof(GLubyte));
		ASSERT(data != NULL);
		
		datalen = 0;
		for (int i = 0; i < nb; i++) {
			if ((i + 1) % 4 || type == GL_RGB) {
				data[datalen++] = texture->data[i];
			}
		}
		glTexImage2D(GL_TEXTURE_2D, 0, type, sizeX, sizeY, sizeBorder, GL_RGB, GL_UNSIGNED_BYTE, data);
	} else {
		if(texture->pvr_header.isCompressed()){
			PVRMipMapLevel mip;
			for(int level = 0; level < texture->pvr_header.MipMapCount; level++){
				if(!texture->pvr_header.getMipMap(level, &mip)){
					ASSERT(!"Failed to get mipmap level");
				}
				glCompressedTexImage2D(
					GL_TEXTURE_2D,
					level,
					texture->pvr_header.getGLInternalFormat(),
					mip.Width,
					mip.Height,
					0,
					mip.size,
					texture->data + mip.offset
				);
			}
		}else{
			PVRMipMapLevel mip;
			for(int level = 0; level < texture->pvr_header.MipMapCount; level++){
				texture->pvr_header.getMipMap(level, &mip);
				glTexImage2D(
					GL_TEXTURE_2D,
					level,
					texture->pvr_header.getGLInternalFormat(),
					mip.Width,
					mip.Height,
					0,
					texture->pvr_header.getGLPixelFormat(),
					texture->pvr_header.getGLPixelType(),
					texture->data + mip.offset
				);
			}
		}
	}
}

void TextureRes::loadData(){
	ASSERT(loadimg == nullptr);
	ImageRec *img = new ImageRec();
	loadimg = (void*) img;
	if(!load_image(filename.c_str(), *img)){
		LOG("WARN: failed to load texture: %s", filename.c_str());
	}
}

void TextureRes::uploadTexture(){
	ASSERT(loadimg != nullptr);
	
	ImageRec *img = (ImageRec*)loadimg;

	if(!img->is_pvr){
		skinsize = img->sizeX;
		GLuint type = GL_RGBA;
		if (img->bpp == 24) {
			type = GL_RGB;
		}
		glDeleteTextures(1, &id);
		glGenTextures(1, &id);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		glBindTexture(GL_TEXTURE_2D, id);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

		if (isSkin) {
			free(data);
			const int nb = img->sizeY * img->sizeX * (img->bpp / 8);
			data = (GLubyte*)malloc(nb * sizeof(GLubyte));
			datalen = 0;
			for (int i = 0; i < nb; i++) {
				if ((i + 1) % 4 || type == GL_RGB) {
					data[datalen++] = img->data[i];
				}
			}
			glTexImage2D(GL_TEXTURE_2D, 0, type, img->sizeX, img->sizeY, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
		} else {
			glTexImage2D(GL_TEXTURE_2D, 0, type, img->sizeX, img->sizeY, 0, type, GL_UNSIGNED_BYTE, img->data);
		}
	}else{
		uploadPVR((void*) loadimg);
	}

	delete img;
	loadimg = nullptr;
}

void TextureRes::load()
{
	ImageRec texture;

	//load image into 'texture'
	if (!load_image(filename.c_str(), texture)) {
		cerr << "Texture " << filename << " loading failed" << endl;
		return;
	}

	if(!texture.is_pvr){
		skinsize = texture.sizeX;
		GLuint type = GL_RGBA;
		if (texture.bpp == 24) {
			type = GL_RGB;
		}

		//VITAGL: TODO
		//glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		glDeleteTextures(1, &id);
		glGenTextures(1, &id);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		glBindTexture(GL_TEXTURE_2D, id);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		if (hasMipmap) {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (trilinear ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_NEAREST));
			//VITAGL: TODO
			//glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
		} else {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		}

		if (isSkin) {
			free(data);
			const int nb = texture.sizeY * texture.sizeX * (texture.bpp / 8);
			data = (GLubyte*)malloc(nb * sizeof(GLubyte));
			datalen = 0;
			for (int i = 0; i < nb; i++) {
				if ((i + 1) % 4 || type == GL_RGB) {
					data[datalen++] = texture.data[i];
				}
			}
			glTexImage2D(GL_TEXTURE_2D, 0, type, texture.sizeX, texture.sizeY, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
		} else {
			glTexImage2D(GL_TEXTURE_2D, 0, type, texture.sizeX, texture.sizeY, 0, type, GL_UNSIGNED_BYTE, texture.data);
		}
	}else{
		uploadPVR((void*)&texture);
	}
}

void TextureRes::bind()
{
	glBindTexture(GL_TEXTURE_2D, id);
}

TextureRes::TextureRes(const string& _filename, bool _hasMipmap)
	: id(0)
	, filename(_filename)
	, hasMipmap(_hasMipmap)
	, isSkin(false)
	, skinsize(0)
	, data(NULL)
	, datalen(0)
	, loadimg(nullptr)
{
	//load();
}

TextureRes::TextureRes(const string& _filename, bool _hasMipmap, GLubyte* array, int* skinsizep)
	: id(0)
	, filename(_filename)
	, hasMipmap(_hasMipmap)
	, isSkin(true)
	, skinsize(0)
	, data(NULL)
	, datalen(0)
	, loadimg(nullptr)
{
	/*
	load();
	*skinsizep = skinsize;
	for (int i = 0; i < datalen; i++) {
		array[i] = data[i];
	}
	*/
}

TextureRes::~TextureRes()
{
	if(data != NULL){
		free(data);
	}
	data = NULL;
	glDeleteTextures(1, &id);
}

void Texture::load(const string& filename, bool hasMipmap)
{
	TextureRes *tr = new TextureRes(Folders::getResourcePath(filename), hasMipmap);
	tex.reset(tr);
	tex->load();
}

void Texture::load(const string& filename, bool hasMipmap, GLubyte* array, int* skinsizep)
{
	TextureRes *tr = new TextureRes(Folders::getResourcePath(filename), hasMipmap, array, skinsizep);
	tex.reset(tr);
	tex->load();
	
	*skinsizep = tex->skinsize;
	for (int i = 0; i < tex->datalen; i++) {
		array[i] = tex->data[i];
	}
}

void Texture::bind()
{
	if (tex) {
		tex->bind();
	} else {
		glBindTexture(GL_TEXTURE_2D, 0);
	}
}

struct LoadImageDataJob: WorkerThread::Job {
	std::shared_ptr<TextureRes> texres;
	LoadImageDataJob(std::shared_ptr<TextureRes> tr):
		Job(),
		texres(tr)
	{
		//--
	}
	~LoadImageDataJob() = default;
	void execute() override {
		texres->loadData();
	}
};

WorkerThread::JobHandle Texture::submitLoadJob(const string& filename, bool hasMipmap){
	TextureRes *tr = new TextureRes(Folders::getResourcePath(filename), hasMipmap);
	tex.reset(tr);
	return WorkerThread::submitJob<LoadImageDataJob>(tex);
}
WorkerThread::JobHandle Texture::submitLoadJob(const string& filename, bool hasMipmap, GLubyte* array, int* skinsizep){
	//TODO
	ASSERT(!"Not implemented");
}

void Texture::upload(){
	tex->uploadTexture();
}

