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

#include "Utils/Folders.hpp"
#include "Utils/ImageIO.hpp"
#include <assert.h>

using namespace std;

extern PVRTexLoader *pvr_loader;
extern bool trilinear;

void TextureRes::uploadPVR(void *pTexture){
    ImageRec *texture = (ImageRec*) pTexture;
    assert(texture->is_pvr);

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
    
    assert(texture->pvr_header.getBorder(0) == texture->pvr_header.getBorder(1) && "PVR texture has uneven borders");
    
    int sizeBorder = texture->pvr_header.getBorder();
    int sizeX = texture->pvr_header.Width + sizeBorder;
    skinsize = sizeX;

    if (isSkin) {
        LOG_TOGGLE(true);

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
        assert(nb == texture->pvr_header.getImageSize());

        //LOG("PVR Skin\n\tborder: %d\n\tsizeX: %d\n\tsizeY: %d\n\tbpp: %d\n\ttype: %d\n\tnb: %d\timgSize: %d",
        //    sizeBorder, sizeX, sizeY, (int) bpp, (int) type, nb, (int) texture->pvr_header.getImageSize()
        //);

        assert(type > 0);

        data = (GLubyte*)malloc(nb * sizeof(GLubyte));
        datalen = 0;
        for (int i = 0; i < nb; i++) {
            if ((i + 1) % 4 || type == GL_RGB) {
                data[datalen++] = texture->data[i];
            }
        }
        glTexImage2D(GL_TEXTURE_2D, 0, type, sizeX, sizeY, sizeBorder, GL_RGB, GL_UNSIGNED_BYTE, data);
    } else {
        if(texture->pvr_header.isCompressed()){
            glCompressedTexImage2D(
                GL_TEXTURE_2D,
                0, //TODO
                texture->pvr_header.getGLInternalFormat(),
                texture->pvr_header.Width,
                texture->pvr_header.Height,
                texture->pvr_header.getBorder(),
                texture->pvr_header.getImageSize(),
                texture->data
            );
        }else{
            glTexImage2D(
                GL_TEXTURE_2D,
                0, //TODO
                texture->pvr_header.getGLInternalFormat(),
                texture->pvr_header.Width + texture->pvr_header.getBorder(0),
                texture->pvr_header.Height + texture->pvr_header.getBorder(1),
                texture->pvr_header.getBorder(),
                texture->pvr_header.getGLPixelFormat(),
                texture->pvr_header.getGLPixelType(),
                texture->data
            );
        }
    }
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
{
    load();
}

TextureRes::TextureRes(const string& _filename, bool _hasMipmap, GLubyte* array, int* skinsizep)
    : id(0)
    , filename(_filename)
    , hasMipmap(_hasMipmap)
    , isSkin(true)
    , skinsize(0)
    , data(NULL)
    , datalen(0)
{
    load();
    *skinsizep = skinsize;
    for (int i = 0; i < datalen; i++) {
        array[i] = data[i];
    }
}

TextureRes::~TextureRes()
{
    if(data != NULL){
        if(is_pvr){
            pvr_loader->freeTexture(data);
        }else{
            free(data);
        }
    }
    data = NULL;
    glDeleteTextures(1, &id);
}

void Texture::load(const string& filename, bool hasMipmap)
{
    tex.reset(new TextureRes(Folders::getResourcePath(filename), hasMipmap));
}

void Texture::load(const string& filename, bool hasMipmap, GLubyte* array, int* skinsizep)
{
    tex.reset(new TextureRes(Folders::getResourcePath(filename), hasMipmap, array, skinsizep));
}

void Texture::bind()
{
    if (tex) {
        tex->bind();
    } else {
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}
