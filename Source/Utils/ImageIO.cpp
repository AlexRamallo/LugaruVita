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

#include "Utils/ImageIO.hpp"
#include "Utils/PVRTexLoader.hpp"
#include "Utils/FileCache.hpp"

#include "Game.hpp"
#include "Thirdparty/microprofile/microprofile.h"
#include "Utils/Folders.hpp"
#include "Utils/Log.h"

#include <jpeglib.h>
#include <png.h>
#include <stdio.h>
#include <assert.h>

#ifndef WIN32
#include <unistd.h>
#endif

#include <physfs.h>

/* These two are needed for screenshot */
extern int kContextWidth;
extern int kContextHeight;
extern PVRTexLoader *pvr_loader;

static bool load_png(const char* fname, ImageRec& tex);
static bool load_jpg(const char* fname, ImageRec& tex);
static bool load_pvr(const char* fname, ImageRec& tex);
static bool save_screenshot_png(const char* fname);

ImageRec::ImageRec()
{
    data = NULL;
}

ImageRec::~ImageRec()
{
    if(is_pvr){
        pvr_loader->freeTexture(data);
    }else{
        free(data);
    }
    data = NULL;
} 

GLuint ImageRec::getWidth() {
    if(is_pvr){
        return info.pvr_header.Width;
    }else{
        return info.img.sizeX;
    }
}
GLuint ImageRec::getHeight() {
    if(is_pvr){
        return info.pvr_header.Height;
    }else{
        return info.img.sizeY;
    }
}
GLuint ImageRec::getBitsPerPixel() {
    if(is_pvr){
        return info.pvr_header.getBitsPerPixel();
    }else{
        return info.img.bpp;
    }
}

int get_filetype(const char *filename){
    static const uint8_t png[8] = {137,80,78,71,13,10,26,10};
    static const uint8_t pvr[3] = {'P', 'V', 'R'};

    //FILE *file = fopen(filename, "rb");
    //FILE *file = FileCache::readFile(filename);
    
    PHYSFS_File *file = PHYSFS_openRead(filename);
    if(file == NULL){
        auto ec = PHYSFS_getLastErrorCode();
        LOG("get_filetype failed to PHYSFS_openRead(%s). Error Code: %d, Msg: %s", filename, (int) ec, PHYSFS_getErrorByCode(ec));
        return -1;
    }

    uint8_t buf[8] = {0,0,0,0,0,0,0,0};

    //if(fread(buf, 1, 8, file) != 8){
    int numread = PHYSFS_readBytes(file, buf, 8);
    if(numread != 8){
        LOG("get_filetype numread is short: %d < 8", numread);
        PHYSFS_close(file);
        return -1;
    }
    PHYSFS_close(file);

    bool is_pvr = true;
    for(int i = 0; i < 3; i++){
        if(buf[i] != pvr[i]){
            is_pvr = false;
            break;
        }
    }
    if(is_pvr){
        return 1;
    }

    bool is_png = true;
    for(int i = 0; i < 8; i++){
        if(buf[i] != png[i]){
            is_png = false;
            break;
        }
    }
    if(is_png){
        return 2;
    }

    //just assume JPEG
    return 3;
}

bool load_image(const char* file_name, ImageRec& tex, bool force_pvr)
{
    MICROPROFILE_SCOPEI("ImageIO", "load_image", 0xffff3456);
    Game::LoadingScreen();

    const char* ptr = strrchr((char*)file_name, '.');
    if (ptr) {
        int type;
        if(force_pvr){
            type = 1;
        }else{
            /*
            if (strcasecmp(ptr + 1, "png") == 0) {
                type = 2;
            } else if (strcasecmp(ptr + 1, "jpg") == 0) {
                type = 3;
            } else if (strcasecmp(ptr + 1, "pvr") == 0) {
                type = 1;
            }
            */
            type = get_filetype(file_name);
        }

        switch(type){
            default:
                LOG("Unrecognized type!");
                return false;

            case 1: //PVR
                LOG("Loading PVR %s", file_name);
                tex.is_pvr = true;
                return load_pvr(file_name, tex);

            case 2: //PNG
                LOG("Loading PNG %s", file_name);
                tex.is_pvr = false;
                return load_png(file_name, tex);

            case 3: //JPEG
                LOG("Loading JPEG %s", file_name);
                tex.is_pvr = false;
                return load_jpg(file_name, tex);
        }
    }

    std::cerr << "Unsupported image type" << std::endl;
    return false;
}

bool save_screenshot(const char* file_name)
{
    const char* ptr = strrchr((char*)file_name, '.');
    if (ptr) {
        if (strcasecmp(ptr + 1, "png") == 0) {
            return save_screenshot_png((Folders::getScreenshotDir() + '/' + file_name).c_str());
        }
    }

    std::cerr << "Unsupported image type" << std::endl;
    return false;
}

static bool load_pvr(const char* file_name, ImageRec& tex){
    MICROPROFILE_SCOPEI("ImageIO", "load_pvr", 0xffff3456);
    return pvr_loader->loadTexture(file_name, &tex.data, &tex.info.pvr_header);
}

struct my_error_mgr
{
    struct jpeg_error_mgr pub; /* "public" fields */
    jmp_buf setjmp_buffer;     /* for return to caller */
};
typedef struct my_error_mgr* my_error_ptr;

static void my_error_exit(j_common_ptr cinfo)
{
    struct my_error_mgr* err = (struct my_error_mgr*)cinfo->err;
    longjmp(err->setjmp_buffer, 1);
}

/* stolen from public domain example.c code in libjpg distribution. */
static bool load_jpg(const char* file_name, ImageRec& tex)
{
    MICROPROFILE_SCOPEI("ImageIO", "load_jpg", 0xffff3456);
    //TODO: calcluate how much we actually need
    ASSERT(tex.data == NULL);
    tex.data = (GLubyte*)malloc(1024 * 1024 * 4);

    struct jpeg_decompress_struct cinfo;
    struct my_error_mgr jerr;
    JSAMPROW buffer[1]; /* Output row buffer */
    int row_stride;     /* physical row width in output buffer */
    errno = 0;
    //FILE* infile = fopen(file_name, "rb");
    //FILE *infile = FileCache::readFile(file_name);
    PHYSFS_File *infile = PHYSFS_openRead(file_name);

    if (infile == NULL) {
        LOG((std::string("Couldn't open file ") + file_name).c_str());
        return false;
    }

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = my_error_exit;
    if (setjmp(jerr.setjmp_buffer)) {
        jpeg_destroy_decompress(&cinfo);
        PHYSFS_close(infile);
        return false;
    }

    jpeg_create_decompress(&cinfo);

    //read entire file to memory
    size_t fbuf_len = PHYSFS_fileLength(infile);
    uint8_t *fbuf = (uint8_t*) malloc(fbuf_len);
    jpeg_mem_src(&cinfo, fbuf, fbuf_len);
    //jpeg_stdio_src(&cinfo, infile);

    (void)jpeg_read_header(&cinfo, TRUE);

    cinfo.out_color_space = JCS_RGB;
    cinfo.quantize_colors = FALSE;
    (void)jpeg_calc_output_dimensions(&cinfo);
    (void)jpeg_start_decompress(&cinfo);

    row_stride = cinfo.output_width * cinfo.output_components;
    tex.info.img.sizeX = cinfo.output_width;
    tex.info.img.sizeY = cinfo.output_height;
    tex.info.img.bpp = 24;

    while (cinfo.output_scanline < cinfo.output_height) {
        buffer[0] = (JSAMPROW)(char*)tex.data +
                    ((cinfo.output_height - 1) - cinfo.output_scanline) * row_stride;
        (void)jpeg_read_scanlines(&cinfo, buffer, 1);
    }

    (void)jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    PHYSFS_close(infile);

    free(fbuf);

    return true;
}

/* stolen from public domain example.c code in libpng distribution. */
static bool load_png(const char* file_name, ImageRec& tex)
{
    MICROPROFILE_SCOPEI("ImageIO", "load_png", 0xffff3456);
    //TODO: calcluate how much we actually need
    ASSERT(tex.data == NULL);
    tex.data = (GLubyte*)malloc(1024 * 1024 * 4);

    if(tex.data == nullptr){
        LOG("Failed to allocate space for tex.data when loading PNG file %s", file_name);
        ASSERT(0);
        return false;
    }

    bool retval = false;
    bool hasalpha = false;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    png_uint_32 width, height;
    int bit_depth, color_type, interlace_type;
    png_byte** row_pointers = NULL;
    errno = 0;
    //FILE* fp = fopen(file_name, "rb");
    //FILE *fp = FileCache::readFile(file_name);

    PHYSFS_File *pfs_fp = PHYSFS_openRead(file_name);
    size_t fbuf_len = PHYSFS_fileLength(pfs_fp);
    uint8_t *fbuf = (uint8_t*) malloc(fbuf_len);

    if(fbuf == nullptr){
        LOG("Failed to allocate space when loading PNG file %s", file_name);
        ASSERT(0);
        return false;
    }

    if(PHYSFS_readBytes(pfs_fp, fbuf, fbuf_len) != fbuf_len){
        LOG("Failed to read PNG file %s", file_name);
        free(fbuf);
        return false;
    }
    FILE *fp = fmemopen(fbuf, fbuf_len, "rb");

    if (fp == NULL) {
        goto png_done;
    }

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL) {
        goto png_done;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
        goto png_done;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        goto png_done;
    }

    png_init_io(png_ptr, fp);
    png_read_png(png_ptr, info_ptr,
                 PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_PACKING,
                 NULL);
    png_get_IHDR(png_ptr, info_ptr, &width, &height,
                 &bit_depth, &color_type, &interlace_type, NULL, NULL);

    if (bit_depth != 8) { // transform SHOULD handle this...
        goto png_done;
    }

    if (color_type & PNG_COLOR_MASK_PALETTE) { // !!! FIXME?
        goto png_done;
    }

    if ((color_type & PNG_COLOR_MASK_COLOR) == 0) { // !!! FIXME?
        goto png_done;
    }

    hasalpha = ((color_type & PNG_COLOR_MASK_ALPHA) != 0);
    row_pointers = png_get_rows(png_ptr, info_ptr);
    if (!row_pointers) {
        goto png_done;
    }

    if (!hasalpha) {
        png_byte* dst = tex.data;
        for (int i = height - 1; i >= 0; i--) {
            png_byte* src = row_pointers[i];
            for (unsigned j = 0; j < width; j++) {
                dst[0] = src[0];
                dst[1] = src[1];
                dst[2] = src[2];
                dst[3] = 0xFF;
                src += 3;
                dst += 4;
            }
        }
    }

    else {
        png_byte* dst = tex.data;
        int pitch = width * 4;
        for (int i = height - 1; i >= 0; i--, dst += pitch) {
            memcpy(dst, row_pointers[i], pitch);
        }
    }

    tex.info.img.sizeX = width;
    tex.info.img.sizeY = height;
    tex.info.img.bpp = 32;
    retval = true;

png_done:
    if (!retval) {
        cerr << "There was a problem loading " << file_name << endl;
    }
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    if (fp) {
        fclose(fp);
    }
    PHYSFS_close(pfs_fp);
    free(fbuf);
    return (retval);
}

static bool save_screenshot_png(const char* file_name)
{
    bool retval = false;
    FILE* fp = NULL;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;

    errno = 0;
    fp = fopen(file_name, "wb");
    if (fp == NULL) {
        perror((std::string("Couldn't open file ") + file_name).c_str());
        return false;
    }

    png_bytep* row_pointers = new png_bytep[kContextHeight];
    png_bytep screenshot = new png_byte[kContextWidth * kContextHeight * 3];
    if ((!screenshot) || (!row_pointers)) {
        goto save_png_done;
    }

    glGetError();
    glReadPixels(0, 0, kContextWidth, kContextHeight,
                 GL_RGB, GL_UNSIGNED_BYTE, screenshot);
    if (glGetError() != GL_NO_ERROR) {
        goto save_png_done;
    }

    for (int i = 0; i < kContextHeight; i++) {
        row_pointers[i] = screenshot + ((kContextWidth * ((kContextHeight - 1) - i)) * 3);
    }

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png_ptr == NULL) {
        goto save_png_done;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
        goto save_png_done;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        goto save_png_done;
    }

    png_init_io(png_ptr, fp);

    if (setjmp(png_jmpbuf(png_ptr))) {
        goto save_png_done;
    }

    png_set_IHDR(png_ptr, info_ptr, kContextWidth, kContextHeight,
                 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    png_write_info(png_ptr, info_ptr);

    if (setjmp(png_jmpbuf(png_ptr))) {
        goto save_png_done;
    }

    png_write_image(png_ptr, row_pointers);

    if (setjmp(png_jmpbuf(png_ptr))) {
        goto save_png_done;
    }

    png_write_end(png_ptr, NULL);
    retval = true;

save_png_done:
    png_destroy_write_struct(&png_ptr, &info_ptr);
    delete[] screenshot;
    delete[] row_pointers;
    if (fp) {
        fclose(fp);
    }
    if (!retval) {
        unlink(file_name);
    }
    return retval;
}
