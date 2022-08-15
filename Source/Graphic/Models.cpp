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

#include "Graphic/Models.hpp"

#include "Game.hpp"
#include "Thirdparty/microprofile/microprofile.h" 
#include "Utils/Folders.hpp"

#include "Thirdparty/vitagl/math_utils.h"
#include "Thirdparty/vitagl/gpu_utils.h"

extern "C" {
    #include <math_neon.h>
}

#include <pthread.h>

extern float multiplier;
extern float viewdistance;
extern XYZ viewer;
extern float fadestart;
extern float texdetail;
extern bool decalstoggle;

static void *alloc_model(size_t size){
#ifdef DRAW_SPEEDHACK
    void *ret = gpu_alloc_mapped(size, VGL_MEM_VRAM);
#else
    void *ret = malloc(size);
#endif
    ASSERT(ret != nullptr && "Failed to allocate memory for model");
    return ret;
}

#define CALCNORM_JOB_SPLIT_DENOM 4
#define NORM_VERTS_JOB_SPLIT_DENOM 3
#define UPDATE_VERT_JOB_SPLIT_DENOM 3


/**
 * Load cache keeps files in memory until cleared, to avoid loading the same
 * file from disk every time it's requested
 * */

int Model::LineCheck(XYZ* p1, XYZ* p2, XYZ* p, XYZ* move, float* rotate)
{
    MICROPROFILE_SCOPEI("Model", "LineCheck", 0x008fff);
    float distance = 0;
    float olddistance = 0;
    int intersecting = 0;
    int firstintersecting = 0;
    XYZ point;

    *p1 = *p1 - *move;
    *p2 = *p2 - *move;
    if (*rotate) {
        *p1 = DoRotation(*p1, 0, -*rotate, 0);
    }
    if (*rotate) {
        *p2 = DoRotation(*p2, 0, -*rotate, 0);
    }
    if (!sphere_line_intersection(p1, p2, &boundingspherecenter, &boundingsphereradius)) {
        return -1;
    }
    firstintersecting = -1;

    for (unsigned int j = 0; j < Triangles.size(); j++) {
        intersecting = LineFacetd(p1, p2, &vertex[Triangles[j].vertex[0]], &vertex[Triangles[j].vertex[1]], &vertex[Triangles[j].vertex[2]], &Triangles[j].facenormal, &point);
        distance = (point.x - p1->x) * (point.x - p1->x) + (point.y - p1->y) * (point.y - p1->y) + (point.z - p1->z) * (point.z - p1->z);
        if ((distance < olddistance || firstintersecting == -1) && intersecting) {
            olddistance = distance;
            firstintersecting = j;
            *p = point;
        }
    }

    if (*rotate) {
        *p = DoRotation(*p, 0, *rotate, 0);
    }
    *p = *p + *move;
    return firstintersecting;
}

int Model::LineCheckPossible(XYZ* p1, XYZ* p2, XYZ* p, XYZ* move, float* rotate)
{
    MICROPROFILE_SCOPEI("Model", "LineCheckPossible", 0x008fff);
    float distance = 0;
    float olddistance = 0;
    int intersecting = 0;
    int firstintersecting = 0;
    XYZ point;

    *p1 = *p1 - *move;
    *p2 = *p2 - *move;
    if (!sphere_line_intersection(p1, p2, &boundingspherecenter, &boundingsphereradius)) {
        return -1;
    }
    firstintersecting = -1;
    if (*rotate) {
        *p1 = DoRotation(*p1, 0, -*rotate, 0);
    }
    if (*rotate) {
        *p2 = DoRotation(*p2, 0, -*rotate, 0);
    }

    for (unsigned int j = 0; j < possible.size(); j++) {
        if (possible[j] < Triangles.size()) {
            intersecting = LineFacetd(p1, p2, &vertex[Triangles[possible[j]].vertex[0]], &vertex[Triangles[possible[j]].vertex[1]], &vertex[Triangles[possible[j]].vertex[2]], &Triangles[possible[j]].facenormal, &point);
            distance = (point.x - p1->x) * (point.x - p1->x) + (point.y - p1->y) * (point.y - p1->y) + (point.z - p1->z) * (point.z - p1->z);
            if ((distance < olddistance || firstintersecting == -1) && intersecting) {
                olddistance = distance;
                firstintersecting = possible[j];
                *p = point;
            }
        }
    }

    if (*rotate) {
        *p = DoRotation(*p, 0, *rotate, 0);
    }
    *p = *p + *move;
    return firstintersecting;
}

int Model::LineCheckSlidePossible(XYZ* p1, XYZ* p2, XYZ* move, float* rotate)
{
    MICROPROFILE_SCOPEI("Model", "LineCheckSlidePossible", 0x008fff);
    float distance = 0;
    float olddistance = 0;
    int intersecting = 0;
    int firstintersecting = 0;
    XYZ point;

    *p1 = *p1 - *move;
    *p2 = *p2 - *move;
    if (!sphere_line_intersection(p1, p2, &boundingspherecenter, &boundingsphereradius)) {
        return -1;
    }
    firstintersecting = -1;
    if (*rotate) {
        *p1 = DoRotation(*p1, 0, -*rotate, 0);
    }
    if (*rotate) {
        *p2 = DoRotation(*p2, 0, -*rotate, 0);
    }

    for (unsigned int j = 0; j < possible.size(); j++) {
        if (possible[j] < Triangles.size()) {
            intersecting = LineFacetd(p1, p2, &vertex[Triangles[possible[j]].vertex[0]], &vertex[Triangles[possible[j]].vertex[1]], &vertex[Triangles[possible[j]].vertex[2]], &Triangles[possible[j]].facenormal, &point);
            distance = (point.x - p1->x) * (point.x - p1->x) + (point.y - p1->y) * (point.y - p1->y) + (point.z - p1->z) * (point.z - p1->z);
            if ((distance < olddistance || firstintersecting == -1) && intersecting) {
                olddistance = distance;
                firstintersecting = possible[j];
            }
        }
    }

    if (firstintersecting > 0) {
        distance = abs((Triangles[firstintersecting].facenormal.x * p2->x) + (Triangles[firstintersecting].facenormal.y * p2->y) + (Triangles[firstintersecting].facenormal.z * p2->z) - ((Triangles[firstintersecting].facenormal.x * vertex[Triangles[firstintersecting].vertex[0]].x) + (Triangles[firstintersecting].facenormal.y * vertex[Triangles[firstintersecting].vertex[0]].y) + (Triangles[firstintersecting].facenormal.z * vertex[Triangles[firstintersecting].vertex[0]].z)));
        *p2 -= Triangles[firstintersecting].facenormal * distance;
    }

    if (*rotate) {
        *p2 = DoRotation(*p2, 0, *rotate, 0);
    }
    *p2 = *p2 + *move;
    return firstintersecting;
}

int Model::SphereCheck(XYZ* p1, float radius, XYZ* p, XYZ* move, float* rotate)
{
    MICROPROFILE_SCOPEI("Model", "SphereCheck", 0x008fff);
    int i = 0;
    float distance = 0;
    float olddistance = 0;
    int intersecting = 0;
    int firstintersecting = 0;
    XYZ point;
    XYZ oldp1;

    firstintersecting = -1;

    oldp1 = *p1;
    *p1 = *p1 - *move;
    if (*rotate) {
        *p1 = DoRotation(*p1, 0, -*rotate, 0);
    }
    if (distsq(p1, &boundingspherecenter) > radius * radius + boundingsphereradius * boundingsphereradius) {
        return -1;
    }

    for (i = 0; i < 4; i++) {
        for (unsigned int j = 0; j < Triangles.size(); j++) {
            intersecting = 0;
            distance = abs((Triangles[j].facenormal.x * p1->x) + (Triangles[j].facenormal.y * p1->y) + (Triangles[j].facenormal.z * p1->z) - ((Triangles[j].facenormal.x * vertex[Triangles[j].vertex[0]].x) + (Triangles[j].facenormal.y * vertex[Triangles[j].vertex[0]].y) + (Triangles[j].facenormal.z * vertex[Triangles[j].vertex[0]].z)));
            if (distance < radius) {
                point = *p1 - Triangles[j].facenormal * distance;
                if (PointInTriangle(&point, Triangles[j].facenormal, &vertex[Triangles[j].vertex[0]], &vertex[Triangles[j].vertex[1]], &vertex[Triangles[j].vertex[2]])) {
                    intersecting = 1;
                }
                if (!intersecting) {
                    intersecting = sphere_line_intersection(&vertex[Triangles[j].vertex[0]], &vertex[Triangles[j].vertex[1]], p1, &radius);
                }
                if (!intersecting) {
                    intersecting = sphere_line_intersection(&vertex[Triangles[j].vertex[1]], &vertex[Triangles[j].vertex[2]], p1, &radius);
                }
                if (!intersecting) {
                    intersecting = sphere_line_intersection(&vertex[Triangles[j].vertex[0]], &vertex[Triangles[j].vertex[2]], p1, &radius);
                }
                if (intersecting) {
                    *p1 += Triangles[j].facenormal * (distance - radius);
                }
            }
            if ((distance < olddistance || firstintersecting == -1) && intersecting) {
                olddistance = distance;
                firstintersecting = j;
                *p = point;
            }
        }
    }
    if (*rotate) {
        *p = DoRotation(*p, 0, *rotate, 0);
    }
    *p = *p + *move;
    if (*rotate) {
        *p1 = DoRotation(*p1, 0, *rotate, 0);
    }
    *p1 += *move;
    return firstintersecting;
}

int Model::SphereCheckPossible(XYZ* p1, float radius, XYZ* move, float* rotate)
{
    MICROPROFILE_SCOPEI("Model", "SphereCheckPossible", 0x008fff);
    float distance = 0;
    float olddistance = 0;
    int intersecting = 0;
    int firstintersecting = 0;
    XYZ point;
    XYZ oldp1;

    firstintersecting = -1;

    oldp1 = *p1;
    *p1 = *p1 - *move;

    possible.clear();

    if (*rotate) {
        *p1 = DoRotation(*p1, 0, -*rotate, 0);
    }
    if (distsq(p1, &boundingspherecenter) > radius * radius + boundingsphereradius * boundingsphereradius) {
        *p1 = oldp1;
        return -1;
    }

    for (unsigned int j = 0; j < Triangles.size(); j++) {
        intersecting = 0;
        distance = abs((Triangles[j].facenormal.x * p1->x) + (Triangles[j].facenormal.y * p1->y) + (Triangles[j].facenormal.z * p1->z) - ((Triangles[j].facenormal.x * vertex[Triangles[j].vertex[0]].x) + (Triangles[j].facenormal.y * vertex[Triangles[j].vertex[0]].y) + (Triangles[j].facenormal.z * vertex[Triangles[j].vertex[0]].z)));
        if (distance < radius) {
            point = *p1 - Triangles[j].facenormal * distance;
            if (PointInTriangle(&point, Triangles[j].facenormal, &vertex[Triangles[j].vertex[0]], &vertex[Triangles[j].vertex[1]], &vertex[Triangles[j].vertex[2]])) {
                intersecting = 1;
            }
            if (!intersecting) {
                intersecting = sphere_line_intersection(&vertex[Triangles[j].vertex[0]], &vertex[Triangles[j].vertex[1]], p1, &radius);
            }
            if (!intersecting) {
                intersecting = sphere_line_intersection(&vertex[Triangles[j].vertex[1]], &vertex[Triangles[j].vertex[2]], p1, &radius);
            }
            if (!intersecting) {
                intersecting = sphere_line_intersection(&vertex[Triangles[j].vertex[0]], &vertex[Triangles[j].vertex[2]], p1, &radius);
            }
            if (intersecting) {
                possible.push_back(j);
            }
        }
        if ((distance < olddistance || firstintersecting == -1) && intersecting) {
            olddistance = distance;
            firstintersecting = j;
        }
    }
    if (*rotate) {
        *p1 = DoRotation(*p1, 0, *rotate, 0);
    }
    *p1 += *move;

    return firstintersecting;
}

void Model::UpdateVertexArray()
{
    MICROPROFILE_SCOPEI("Model", "UpdateVertexArray", 0x008fff);
    if (type != normaltype && type != decalstype) {
        return;
    }

    if (flat) {
        for (unsigned int i = 0; i < Triangles.size(); i++) {
            unsigned int j = i * 24;
            vArray[j + 0] = Triangles[i].gx[0];
            vArray[j + 1] = Triangles[i].gy[0];
            vArray[j + 2] = Triangles[i].facenormal.x * -1;
            vArray[j + 3] = Triangles[i].facenormal.y * -1;
            vArray[j + 4] = Triangles[i].facenormal.z * -1;
            vArray[j + 5] = vertex[Triangles[i].vertex[0]].x;
            vArray[j + 6] = vertex[Triangles[i].vertex[0]].y;
            vArray[j + 7] = vertex[Triangles[i].vertex[0]].z;

            vArray[j + 8] = Triangles[i].gx[1];
            vArray[j + 9] = Triangles[i].gy[1];
            vArray[j + 10] = Triangles[i].facenormal.x * -1;
            vArray[j + 11] = Triangles[i].facenormal.y * -1;
            vArray[j + 12] = Triangles[i].facenormal.z * -1;
            vArray[j + 13] = vertex[Triangles[i].vertex[1]].x;
            vArray[j + 14] = vertex[Triangles[i].vertex[1]].y;
            vArray[j + 15] = vertex[Triangles[i].vertex[1]].z;

            vArray[j + 16] = Triangles[i].gx[2];
            vArray[j + 17] = Triangles[i].gy[2];
            vArray[j + 18] = Triangles[i].facenormal.x * -1;
            vArray[j + 19] = Triangles[i].facenormal.y * -1; 
            vArray[j + 20] = Triangles[i].facenormal.z * -1;
            vArray[j + 21] = vertex[Triangles[i].vertex[2]].x;
            vArray[j + 22] = vertex[Triangles[i].vertex[2]].y;
            vArray[j + 23] = vertex[Triangles[i].vertex[2]].z;
        }
    } else {
        for (unsigned int i = 0; i < Triangles.size(); i++) {
            unsigned int j = i * 24;
            vArray[j + 0] = Triangles[i].gx[0];
            vArray[j + 1] = Triangles[i].gy[0];
            vArray[j + 2] = normals[Triangles[i].vertex[0]].x;
            vArray[j + 3] = normals[Triangles[i].vertex[0]].y;
            vArray[j + 4] = normals[Triangles[i].vertex[0]].z;
            vArray[j + 5] = vertex[Triangles[i].vertex[0]].x;
            vArray[j + 6] = vertex[Triangles[i].vertex[0]].y;
            vArray[j + 7] = vertex[Triangles[i].vertex[0]].z;

            vArray[j + 8] = Triangles[i].gx[1];
            vArray[j + 9] = Triangles[i].gy[1];
            vArray[j + 10] = normals[Triangles[i].vertex[1]].x;
            vArray[j + 11] = normals[Triangles[i].vertex[1]].y;
            vArray[j + 12] = normals[Triangles[i].vertex[1]].z;
            vArray[j + 13] = vertex[Triangles[i].vertex[1]].x;
            vArray[j + 14] = vertex[Triangles[i].vertex[1]].y;
            vArray[j + 15] = vertex[Triangles[i].vertex[1]].z;

            vArray[j + 16] = Triangles[i].gx[2];
            vArray[j + 17] = Triangles[i].gy[2];
            vArray[j + 18] = normals[Triangles[i].vertex[2]].x;
            vArray[j + 19] = normals[Triangles[i].vertex[2]].y;
            vArray[j + 20] = normals[Triangles[i].vertex[2]].z;
            vArray[j + 21] = vertex[Triangles[i].vertex[2]].x;
            vArray[j + 22] = vertex[Triangles[i].vertex[2]].y;
            vArray[j + 23] = vertex[Triangles[i].vertex[2]].z;
        }
    }
}

void Model::UpdateVertexArrayNoTex()
{
    MICROPROFILE_SCOPEI("Model", "UpdateVertexArrayNoTex", 0x008fff);
    if (type != normaltype && type != decalstype) {
        return;
    }

    if (flat) {
        for (unsigned int i = 0; i < Triangles.size(); i++) {
            unsigned int j = i * 24;
            vArray[j + 2] = Triangles[i].facenormal.x * -1;
            vArray[j + 3] = Triangles[i].facenormal.y * -1;
            vArray[j + 4] = Triangles[i].facenormal.z * -1;
            vArray[j + 5] = vertex[Triangles[i].vertex[0]].x;
            vArray[j + 6] = vertex[Triangles[i].vertex[0]].y;
            vArray[j + 7] = vertex[Triangles[i].vertex[0]].z;

            vArray[j + 10] = Triangles[i].facenormal.x * -1;
            vArray[j + 11] = Triangles[i].facenormal.y * -1;
            vArray[j + 12] = Triangles[i].facenormal.z * -1;
            vArray[j + 13] = vertex[Triangles[i].vertex[1]].x;
            vArray[j + 14] = vertex[Triangles[i].vertex[1]].y;
            vArray[j + 15] = vertex[Triangles[i].vertex[1]].z;

            vArray[j + 18] = Triangles[i].facenormal.x * -1;
            vArray[j + 19] = Triangles[i].facenormal.y * -1;
            vArray[j + 20] = Triangles[i].facenormal.z * -1;
            vArray[j + 21] = vertex[Triangles[i].vertex[2]].x;
            vArray[j + 22] = vertex[Triangles[i].vertex[2]].y;
            vArray[j + 23] = vertex[Triangles[i].vertex[2]].z;
        }
    } else {
        for (unsigned int i = 0; i < Triangles.size(); i++) {
            unsigned int j = i * 24;
            vArray[j + 2] = normals[Triangles[i].vertex[0]].x;
            vArray[j + 3] = normals[Triangles[i].vertex[0]].y;
            vArray[j + 4] = normals[Triangles[i].vertex[0]].z;
            vArray[j + 5] = vertex[Triangles[i].vertex[0]].x;
            vArray[j + 6] = vertex[Triangles[i].vertex[0]].y;
            vArray[j + 7] = vertex[Triangles[i].vertex[0]].z;

            vArray[j + 10] = normals[Triangles[i].vertex[1]].x;
            vArray[j + 11] = normals[Triangles[i].vertex[1]].y;
            vArray[j + 12] = normals[Triangles[i].vertex[1]].z;
            vArray[j + 13] = vertex[Triangles[i].vertex[1]].x;
            vArray[j + 14] = vertex[Triangles[i].vertex[1]].y;
            vArray[j + 15] = vertex[Triangles[i].vertex[1]].z;

            vArray[j + 18] = normals[Triangles[i].vertex[2]].x;
            vArray[j + 19] = normals[Triangles[i].vertex[2]].y;
            vArray[j + 20] = normals[Triangles[i].vertex[2]].z;
            vArray[j + 21] = vertex[Triangles[i].vertex[2]].x;
            vArray[j + 22] = vertex[Triangles[i].vertex[2]].y;
            vArray[j + 23] = vertex[Triangles[i].vertex[2]].z;
        }
    }
}

void Model::UpdateVertexArrayNoTexNoNorm()
{
    MICROPROFILE_SCOPEI("Model", "UpdateVertexArrayNoTexNoNorm", 0x008fff);

    if (type != normaltype && type != decalstype) {
        return;
    }

    for (unsigned int i = 0; i < Triangles.size(); i++) {
        unsigned int j = i * 24;
        vArray[j + 5] = vertex[Triangles[i].vertex[0]].x;
        vArray[j + 6] = vertex[Triangles[i].vertex[0]].y;
        vArray[j + 7] = vertex[Triangles[i].vertex[0]].z;

        vArray[j + 13] = vertex[Triangles[i].vertex[1]].x;
        vArray[j + 14] = vertex[Triangles[i].vertex[1]].y;
        vArray[j + 15] = vertex[Triangles[i].vertex[1]].z;

        vArray[j + 21] = vertex[Triangles[i].vertex[2]].x;
        vArray[j + 22] = vertex[Triangles[i].vertex[2]].y;
        vArray[j + 23] = vertex[Triangles[i].vertex[2]].z;
    }
}

struct LoadModelJob: WorkerThread::Job {
    std::string fname;
    Model *model;
    int type;
    LoadModelJob(std::string f, Model *m, int t):
        Job(),
        fname(f),
        model(m),
        type(t)
    {
        //--
    }
    ~LoadModelJob() = default;

    void execute() override {
        switch(type){
            default:
                model->loadnotex(fname);
                break;
            case 1:
                model->load(fname);
                break;
            case 2:
                model->loaddecal(fname);
                break;
            case 3:
                model->loadraw(fname);
                break;
        }
    }
};

WorkerThread::JobHandle Model::submitLoadnotex(const std::string &filename){
    return WorkerThread::submitJob<LoadModelJob>(filename, this, 0);
}
WorkerThread::JobHandle Model::submitLoad(const std::string &filename){
    return WorkerThread::submitJob<LoadModelJob>(filename, this, 1);
}
WorkerThread::JobHandle Model::submitLoadDecal(const std::string &filename){
    return WorkerThread::submitJob<LoadModelJob>(filename, this, 2);
}
WorkerThread::JobHandle Model::submitLoadRaw(const std::string &filename){
    return WorkerThread::submitJob<LoadModelJob>(filename, this, 3);
}

struct ModelCacheEntry {
    const std::string filename;
    const Model *model;
    ModelCacheEntry(const std::string &f, Model *m):
        filename(f),
        model(m)
    {
        //--
    }

    ~ModelCacheEntry(){
        if(model != nullptr){
            delete model;
            model = nullptr;
        }
    }
};

static pthread_mutex_t mtxCache;
std::vector<std::shared_ptr<ModelCacheEntry>> modelCache;
static bool did_init_once = false;
void Model::initModelCache(){
    ASSERT(modelCache.size() == 0);
    if(!did_init_once){
        did_init_once = true;
        if(pthread_mutex_init(&mtxCache, NULL)){
            ASSERT(!"Failed to initialize model cache mutex");
        }
    }
}

void Model::clearModelCache(){
    modelCache.clear();
}

static bool getModelCache(Model *model, const std::string &filename){
    if(pthread_mutex_lock(&mtxCache)){
        ASSERT(!"Failed to lock model cache mutex");
        return false;
    }

    std::shared_ptr<ModelCacheEntry> cached = nullptr;
    for(std::shared_ptr<ModelCacheEntry> cache: modelCache){
        if(cache->model->type == model->type){
            if(cache->filename == filename){
                cached = cache;
                break;
            }
        }
    }

    if(cached == nullptr){
        if(pthread_mutex_unlock(&mtxCache)){
            ASSERT(!"Failed to unlock model cache mutex");
            return false;
        }

        Model *cm = new Model();
        bool ok = false;

        switch(model->type){
            default:
                ASSERT(!"Bad model type");
                pthread_mutex_unlock(&mtxCache);
                return false;
        
            case notextype:
                ok = cm->loadnotex(filename, false);
                break;

            case decalstype:
                ok = cm->loaddecal(filename, false);
                break;

            case rawtype:
                ok = cm->loadraw(filename, false);
                break;

            case normaltype:
                ok = cm->load(filename, false);
                break;
        }
        
        if(pthread_mutex_lock(&mtxCache)){
            ASSERT(!"Failed to lock model cache mutex");
            return false;
        }

        if(ok){
            modelCache.push_back(std::make_shared<ModelCacheEntry>(filename, cm));
            cached = modelCache.back();
        }else{
            delete cm;
            cached = nullptr;
        }
    }
    
    if(pthread_mutex_unlock(&mtxCache)){
        ASSERT(!"Failed to unlock model cache mutex");
        return false;
    }

    if(cached == nullptr){
        return false;
    }

    ASSERT(model->vertex == nullptr);
    ASSERT(model->normals == nullptr);
    ASSERT(model->Triangles.size() == 0);
    ASSERT(model->vertexNum == 0);

    int numVert = cached->model->vertexNum;
    model->vertexNum = numVert;
    model->vertex = (XYZ*)malloc(sizeof(XYZ) * numVert);
    ASSERT(model->vertex != nullptr);

    memcpy(model->vertex, cached->model->vertex, sizeof(XYZ) * numVert);
    
    if(cached->model->type == decalstype || cached->model->type == normaltype){
        model->normals = (XYZ*)malloc(sizeof(XYZ) * numVert);
        memcpy(model->normals, cached->model->normals, sizeof(XYZ) * numVert);
    }

    model->Triangles = cached->model->Triangles;

    ASSERT(model->vertexNum == cached->model->vertexNum);
    ASSERT(model->Triangles.size() == cached->model->Triangles.size());
    ASSERT(&model->Triangles[0] != &cached->model->Triangles[0]);
    return true;
}

bool Model::loadnotex(const std::string& filename, bool use_cache)
{
    MICROPROFILE_SCOPEI("Model", "loadnotex", 0x008fff);
    PHYSFS_File* tfile;
    long i;
    short triangleNum;

    type = notextype;
    color = 0;
    
    deallocate();
    possible.clear();

    if(use_cache){
        if(!getModelCache(this, filename)){
            ASSERT(!"Failed to load notex model!");
            return false;
        }
        
        owner = (int*)malloc(sizeof(int) * vertexNum);
        //vArray = (GLfloat*)malloc(sizeof(GLfloat) * Triangles.size() * 24);
        vArray = (GLfloat*)alloc_model(sizeof(GLfloat) * Triangles.size() * 24);
        vgl_array = true;
    }else{
        vgl_array = false;
        tfile = PHYSFS_openRead(Folders::getResourcePath(filename).c_str());
        // read model settings
        funpackf(tfile, "Bs Bs", &vertexNum, &triangleNum);
        // read the model data
        owner = (int*)malloc(sizeof(int) * vertexNum);
        vertex = (XYZ*)malloc(sizeof(XYZ) * vertexNum);
        Triangles.resize(triangleNum);
        vArray = (GLfloat*)malloc(sizeof(GLfloat) * triangleNum * 24);

        for (i = 0; i < vertexNum; i++) {
            funpackf(tfile, "Bf Bf Bf", &vertex[i].x, &vertex[i].y, &vertex[i].z);
        }

        for (i = 0; i < triangleNum; i++) {
            short vertex[6];
            funpackf(tfile, "Bs Bs Bs Bs Bs Bs", &vertex[0], &vertex[1], &vertex[2], &vertex[3], &vertex[4], &vertex[5]);
            Triangles[i].vertex[0] = vertex[0];
            Triangles[i].vertex[1] = vertex[2];
            Triangles[i].vertex[2] = vertex[4];
            funpackf(tfile, "Bf Bf Bf", &Triangles[i].gx[0], &Triangles[i].gx[1], &Triangles[i].gx[2]);
            funpackf(tfile, "Bf Bf Bf", &Triangles[i].gy[0], &Triangles[i].gy[1], &Triangles[i].gy[2]);
        }
        PHYSFS_close(tfile);
    }
    UpdateVertexArray();

    for (i = 0; i < vertexNum; i++) {
        owner[i] = -1;
    }

    boundingsphereradius = 0;
    for (i = 0; i < vertexNum; i++) {
        for (int j = 0; j < vertexNum; j++) {
            if (j != i && distsq(&vertex[j], &vertex[i]) / 2 > boundingsphereradius) {
                boundingsphereradius = distsq(&vertex[j], &vertex[i]) / 2;
                boundingspherecenter = (vertex[i] + vertex[j]) / 2;
            }
        }
    }
    boundingsphereradius = fast_sqrt(boundingsphereradius);

    return true;
}

bool Model::load(const std::string& filename, bool use_cache)
{
    MICROPROFILE_SCOPEI("Model", "load", 0x008fff);
    PHYSFS_File* tfile;
    long i;
    short triangleNum;

    LOGFUNC;

    LOG("Loading model...%s", filename.c_str());

    //Game::LoadingScreen();

    type = normaltype;
    color = 0;
    
    deallocate();
    possible.clear();

    if(use_cache){
        if(!getModelCache(this, filename)){
            ASSERT(!"Failed to load normal model!");
            return false;
        }

        owner = (int*)malloc(sizeof(int) * vertexNum);
        //vArray = (GLfloat*)malloc(sizeof(GLfloat) * Triangles.size() * 24);
        vArray = (GLfloat*)alloc_model(sizeof(GLfloat) * Triangles.size() * 24);
        vgl_array = true;
    }else{
        vgl_array = false;
        tfile = PHYSFS_openRead(Folders::getResourcePath(filename).c_str());

        // read model settings
        funpackf(tfile, "Bs Bs", &vertexNum, &triangleNum);

        {
            MICROPROFILE_SCOPEI("Model", "allocbufs", 0x008fff);
            owner = (int*)malloc(sizeof(int) * vertexNum);
            vertex = (XYZ*)malloc(sizeof(XYZ) * vertexNum);
            normals = (XYZ*)malloc(sizeof(XYZ) * vertexNum);
            Triangles.resize(triangleNum);
            vArray = (GLfloat*)malloc(sizeof(GLfloat) * triangleNum * 24);
        }
        {
            MICROPROFILE_SCOPEI("Model", "unpackverts", 0x008fff);
            for (i = 0; i < vertexNum; i++) {
                funpackf(tfile, "Bf Bf Bf", &vertex[i].x, &vertex[i].y, &vertex[i].z);
            }
        }
        {
            MICROPROFILE_SCOPEI("Model", "unpacktris", 0x008fff);
            for (i = 0; i < triangleNum; i++) {
                short vertex[6];
                funpackf(tfile, "Bs Bs Bs Bs Bs Bs", &vertex[0], &vertex[1], &vertex[2], &vertex[3], &vertex[4], &vertex[5]);
                Triangles[i].vertex[0] = vertex[0];
                Triangles[i].vertex[1] = vertex[2];
                Triangles[i].vertex[2] = vertex[4];

                funpackf(tfile, "Bf Bf Bf", &Triangles[i].gx[0], &Triangles[i].gx[1], &Triangles[i].gx[2]);
                funpackf(tfile, "Bf Bf Bf", &Triangles[i].gy[0], &Triangles[i].gy[1], &Triangles[i].gy[2]);
            }
        }
        PHYSFS_close(tfile);
    }
    modelTexture.xsz = 0;

    UpdateVertexArray();

    for (i = 0; i < vertexNum; i++) {
        owner[i] = -1;
    }

    int j;
    boundingsphereradius = 0;
    for (i = 0; i < vertexNum; i++) {
        for (j = 0; j < vertexNum; j++) {
            if (j != i && distsq(&vertex[j], &vertex[i]) / 2 > boundingsphereradius) {
                boundingsphereradius = distsq(&vertex[j], &vertex[i]) / 2;
                boundingspherecenter = (vertex[i] + vertex[j]) / 2;
            }
        }
    }
    boundingsphereradius = fast_sqrt(boundingsphereradius);

    return true;
}

bool Model::loaddecal(const std::string& filename, bool use_cache)
{
    MICROPROFILE_SCOPEI("Model", "loaddecal", 0x008fff);
    PHYSFS_File* tfile;
    long i, j;
    short triangleNum;

    LOGFUNC;

    LOG("Loading decal... %s", Folders::getResourcePath(filename).c_str());

    type = decalstype;
    color = 0;

    deallocate();
    possible.clear();

    if(use_cache){
        if(!getModelCache(this, filename)){
            ASSERT(!"Failed to load normal model!");
            return false;
        }

        owner = (int*)malloc(sizeof(int) * vertexNum);
        //vArray = (GLfloat*)malloc(sizeof(GLfloat) * Triangles.size() * 24);
        vArray = (GLfloat*)alloc_model(sizeof(GLfloat) * Triangles.size() * 24);
        vgl_array = true;
    }else{
        vgl_array = false;
        tfile = PHYSFS_openRead(Folders::getResourcePath(filename).c_str());
        // read model settings
        funpackf(tfile, "Bs Bs", &vertexNum, &triangleNum);
        // read the model data
        owner = (int*)malloc(sizeof(int) * vertexNum);
        vertex = (XYZ*)malloc(sizeof(XYZ) * vertexNum);
        normals = (XYZ*)malloc(sizeof(XYZ) * vertexNum);
        Triangles.resize(triangleNum);
        vArray = (GLfloat*)malloc(sizeof(GLfloat) * triangleNum * 24);

        for (i = 0; i < vertexNum; i++) {
            funpackf(tfile, "Bf Bf Bf", &vertex[i].x, &vertex[i].y, &vertex[i].z);
        }

        for (i = 0; i < triangleNum; i++) {
            short vertex[6];
            funpackf(tfile, "Bs Bs Bs Bs Bs Bs", &vertex[0], &vertex[1], &vertex[2], &vertex[3], &vertex[4], &vertex[5]);
            Triangles[i].vertex[0] = vertex[0];
            Triangles[i].vertex[1] = vertex[2];
            Triangles[i].vertex[2] = vertex[4];
            funpackf(tfile, "Bf Bf Bf", &Triangles[i].gx[0], &Triangles[i].gx[1], &Triangles[i].gx[2]);
            funpackf(tfile, "Bf Bf Bf", &Triangles[i].gy[0], &Triangles[i].gy[1], &Triangles[i].gy[2]);
        }
        PHYSFS_close(tfile);
    }
    modelTexture.xsz = 0;

    UpdateVertexArray();

    for (i = 0; i < vertexNum; i++) {
        owner[i] = -1;
    }

    boundingsphereradius = 0;
    for (i = 0; i < vertexNum; i++) {
        for (j = 0; j < vertexNum; j++) {
            if (j != i && distsq(&vertex[j], &vertex[i]) / 2 > boundingsphereradius) {
                boundingsphereradius = distsq(&vertex[j], &vertex[i]) / 2;
                boundingspherecenter = (vertex[i] + vertex[j]) / 2;
            }
        }
    }
    boundingsphereradius = fast_sqrt(boundingsphereradius);

    return true;
}

bool Model::loadraw(const std::string& filename, bool use_cache)
{
    MICROPROFILE_SCOPEI("Model", "loadraw", 0x008fff);
    PHYSFS_File* tfile;
    long i;
    short triangleNum;

    LOGFUNC;

    LOG(std::string("Loading raw...") + filename);

    type = rawtype;
    color = 0;
    
    deallocate();
    possible.clear();

    if(use_cache){
        if(!getModelCache(this, filename)){
            ASSERT(!"Failed to load normal model!");
            return false;
        }
        owner = (int*)malloc(sizeof(int) * vertexNum);
        //vArray = (GLfloat*)malloc(sizeof(GLfloat) * Triangles.size() * 24);
        vArray = (GLfloat*)alloc_model(sizeof(GLfloat) * Triangles.size() * 24);
        vgl_array = true;
    }else{
        vgl_array = false;
        tfile = PHYSFS_openRead(Folders::getResourcePath(filename).c_str());

        // read model settings
        funpackf(tfile, "Bs Bs", &vertexNum, &triangleNum);

        owner = (int*)malloc(sizeof(int) * vertexNum);
        vertex = (XYZ*)malloc(sizeof(XYZ) * vertexNum);
        Triangles.resize(triangleNum);
        vArray = (GLfloat*)malloc(sizeof(GLfloat) * triangleNum * 24);

        for (i = 0; i < vertexNum; i++) {
            funpackf(tfile, "Bf Bf Bf", &vertex[i].x, &vertex[i].y, &vertex[i].z);
        }

        for (i = 0; i < triangleNum; i++) {
            short vertex[6];
            funpackf(tfile, "Bs Bs Bs Bs Bs Bs", &vertex[0], &vertex[1], &vertex[2], &vertex[3], &vertex[4], &vertex[5]);
            Triangles[i].vertex[0] = vertex[0];
            Triangles[i].vertex[1] = vertex[2];
            Triangles[i].vertex[2] = vertex[4];
            funpackf(tfile, "Bf Bf Bf", &Triangles[i].gx[0], &Triangles[i].gx[1], &Triangles[i].gx[2]);
            funpackf(tfile, "Bf Bf Bf", &Triangles[i].gy[0], &Triangles[i].gy[1], &Triangles[i].gy[2]);
        }

        PHYSFS_close(tfile);
    }

    for (i = 0; i < vertexNum; i++) {
        owner[i] = -1;
    }

    return true;
}

void Model::UniformTexCoords()
{
    MICROPROFILE_SCOPEI("Model", "UniformTexCoords", 0x008fff);
    for (unsigned int i = 0; i < Triangles.size(); i++) {
        Triangles[i].gy[0] = vertex[Triangles[i].vertex[0]].y;
        Triangles[i].gy[1] = vertex[Triangles[i].vertex[1]].y;
        Triangles[i].gy[2] = vertex[Triangles[i].vertex[2]].y;
        Triangles[i].gx[0] = vertex[Triangles[i].vertex[0]].x;
        Triangles[i].gx[1] = vertex[Triangles[i].vertex[1]].x;
        Triangles[i].gx[2] = vertex[Triangles[i].vertex[2]].x;
    }
    UpdateVertexArray();
}

void Model::FlipTexCoords()
{
    MICROPROFILE_SCOPEI("Model", "FlipTexCoords", 0x008fff);
    for (unsigned int i = 0; i < Triangles.size(); i++) {
        Triangles[i].gy[0] = -Triangles[i].gy[0];
        Triangles[i].gy[1] = -Triangles[i].gy[1];
        Triangles[i].gy[2] = -Triangles[i].gy[2];
    }
    UpdateVertexArray();
}

void Model::ScaleTexCoords(float howmuch)
{
    MICROPROFILE_SCOPEI("Model", "ScaleTexCoords", 0x008fff);
    for (unsigned int i = 0; i < Triangles.size(); i++) {
        Triangles[i].gx[0] *= howmuch;
        Triangles[i].gx[1] *= howmuch;
        Triangles[i].gx[2] *= howmuch;
        Triangles[i].gy[0] *= howmuch;
        Triangles[i].gy[1] *= howmuch;
        Triangles[i].gy[2] *= howmuch;
    }
    UpdateVertexArray();
}

void Model::Scale(float xscale, float yscale, float zscale)
{
    MICROPROFILE_SCOPEI("Model", "Scale", 0x008fff);
    int i;
    for (i = 0; i < vertexNum; i++) {
        vertex[i].x *= xscale;
        vertex[i].y *= yscale;
        vertex[i].z *= zscale;
    }
    UpdateVertexArray();

    int j;

    boundingsphereradius = 0;
    for (i = 0; i < vertexNum; i++) {
        for (j = 0; j < vertexNum; j++) {
            if (j != i && distsq(&vertex[j], &vertex[i]) / 2 > boundingsphereradius) {
                boundingsphereradius = distsq(&vertex[j], &vertex[i]) / 2;
                boundingspherecenter = (vertex[i] + vertex[j]) / 2;
            }
        }
    }
    boundingsphereradius = fast_sqrt(boundingsphereradius);
}

void Model::ScaleNormals(float xscale, float yscale, float zscale)
{
    MICROPROFILE_SCOPEI("Model", "ScaleNormals", 0x008fff);
    if (type != normaltype && type != decalstype) {
        return;
    }

    for (int i = 0; i < vertexNum; i++) {
        normals[i].x *= xscale;
        normals[i].y *= yscale;
        normals[i].z *= zscale;
    }
    for (unsigned int i = 0; i < Triangles.size(); i++) {
        Triangles[i].facenormal.x *= xscale;
        Triangles[i].facenormal.y *= yscale;
        Triangles[i].facenormal.z *= zscale;
    }
    UpdateVertexArray();
}

void Model::Translate(float xtrans, float ytrans, float ztrans)
{
    MICROPROFILE_SCOPEI("Model", "Translate", 0x008fff);
    int i;
    for (i = 0; i < vertexNum; i++) {
        vertex[i].x += xtrans;
        vertex[i].y += ytrans;
        vertex[i].z += ztrans;
    }
    UpdateVertexArray();

    int j;
    boundingsphereradius = 0;
    for (i = 0; i < vertexNum; i++) {
        for (j = 0; j < vertexNum; j++) {
            if (j != i && distsq(&vertex[j], &vertex[i]) / 2 > boundingsphereradius) {
                boundingsphereradius = distsq(&vertex[j], &vertex[i]) / 2;
                boundingspherecenter = (vertex[i] + vertex[j]) / 2;
            }
        }
    }
    boundingsphereradius = fast_sqrt(boundingsphereradius);
}

void Model::Rotate(float xang, float yang, float zang)
{
    MICROPROFILE_SCOPEI("Model", "Rotate", 0x008fff);
    int i;
    for (i = 0; i < vertexNum; i++) {
        vertex[i] = DoRotation(vertex[i], xang, yang, zang);
    }
    UpdateVertexArray();

    int j;
    boundingsphereradius = 0;
    for (i = 0; i < vertexNum; i++) {
        for (j = 0; j < vertexNum; j++) {
            if (j != i && distsq(&vertex[j], &vertex[i]) / 2 > boundingsphereradius) {
                boundingsphereradius = distsq(&vertex[j], &vertex[i]) / 2;
                boundingspherecenter = (vertex[i] + vertex[j]) / 2;
            }
        }
    }
    boundingsphereradius = fast_sqrt(boundingsphereradius);
}

struct CalculateNormalsJob: WorkerThread::Job {
    int start_idx, end_idx;
    Model *model;
    bool facenormalise;
    CalculateNormalsJob(int s, int e, Model *m, bool fn):
        Job(),
        start_idx(s),
        end_idx(e),
        model(m),
        facenormalise(fn)
    {
        //--
    }
    void execute() override {
        MICROPROFILE_SCOPEI("Model", "CalculateNormalsJob", 0xe4a77c4);
        std::vector<TexturedTriangle> &Triangles = model->Triangles;
        XYZ *vertex = model->vertex;
        XYZ *normals = model->normals;

        for(int i = start_idx; i <= end_idx; i++){
            XYZ v0 = vertex[Triangles[i].vertex[1]] - vertex[Triangles[i].vertex[0]];
            XYZ v1 = vertex[Triangles[i].vertex[2]] - vertex[Triangles[i].vertex[0]];
            cross3_neon(&v0.x, &v1.x, &Triangles[i].facenormal.x);

            normals[Triangles[i].vertex[0]].x += Triangles[i].facenormal.x;
            normals[Triangles[i].vertex[0]].y += Triangles[i].facenormal.y;
            normals[Triangles[i].vertex[0]].z += Triangles[i].facenormal.z;

            normals[Triangles[i].vertex[1]].x += Triangles[i].facenormal.x;
            normals[Triangles[i].vertex[1]].y += Triangles[i].facenormal.y;
            normals[Triangles[i].vertex[1]].z += Triangles[i].facenormal.z;

            normals[Triangles[i].vertex[2]].x += Triangles[i].facenormal.x;
            normals[Triangles[i].vertex[2]].y += Triangles[i].facenormal.y;
            normals[Triangles[i].vertex[2]].z += Triangles[i].facenormal.z;
            if (facenormalise) {
                Normalise(&Triangles[i].facenormal);
            }
        }
    }
};

struct NormalizeVertsJob: WorkerThread::Job {
    int start_idx, end_idx;
    Model *model;
    NormalizeVertsJob(int s, int e, Model *m):
        Job(),
        start_idx(s),
        end_idx(e),
        model(m)
    {
        //--
    }
    void execute() override {
        MICROPROFILE_SCOPEI("Model", "NormalizeVertsJob", 0xe4a77c4);
        for (int i = start_idx; i <= end_idx; i++) {
            Normalise(&model->normals[i]);
            model->normals[i] *= -1;
        }
    }
};

struct UpdateVertexJob: WorkerThread::Job {
    unsigned int type, start_idx, end_idx;
    Model *model;
    UpdateVertexJob(int t, int s, int e, Model *m):
        Job(),
        type(t),
        start_idx(s),
        end_idx(e),
        model(m)
    {
        //--
    }
    void update_notex(std::vector<TexturedTriangle> &Triangles, XYZ *vertex, XYZ *normals, GLfloat *vArray){
        MICROPROFILE_SCOPEI("Model", "update_notex", 0xe4a77c4);
        if (model->type != normaltype && model->type != decalstype) {
            return;
        }
        if (model->flat) {
            for (unsigned int i = start_idx; i <= end_idx; i++) {
                unsigned int j = i * 24;
                vArray[j + 2] = Triangles[i].facenormal.x * -1;
                vArray[j + 3] = Triangles[i].facenormal.y * -1;
                vArray[j + 4] = Triangles[i].facenormal.z * -1;
                vArray[j + 5] = vertex[Triangles[i].vertex[0]].x;
                vArray[j + 6] = vertex[Triangles[i].vertex[0]].y;
                vArray[j + 7] = vertex[Triangles[i].vertex[0]].z;

                vArray[j + 10] = Triangles[i].facenormal.x * -1;
                vArray[j + 11] = Triangles[i].facenormal.y * -1;
                vArray[j + 12] = Triangles[i].facenormal.z * -1;
                vArray[j + 13] = vertex[Triangles[i].vertex[1]].x;
                vArray[j + 14] = vertex[Triangles[i].vertex[1]].y;
                vArray[j + 15] = vertex[Triangles[i].vertex[1]].z;

                vArray[j + 18] = Triangles[i].facenormal.x * -1;
                vArray[j + 19] = Triangles[i].facenormal.y * -1;
                vArray[j + 20] = Triangles[i].facenormal.z * -1;
                vArray[j + 21] = vertex[Triangles[i].vertex[2]].x;
                vArray[j + 22] = vertex[Triangles[i].vertex[2]].y;
                vArray[j + 23] = vertex[Triangles[i].vertex[2]].z;
            }
        } else {
            for (unsigned int i = start_idx; i <= end_idx; i++) {
                unsigned int j = i * 24;
                vArray[j + 2] = normals[Triangles[i].vertex[0]].x;
                vArray[j + 3] = normals[Triangles[i].vertex[0]].y;
                vArray[j + 4] = normals[Triangles[i].vertex[0]].z;
                vArray[j + 5] = vertex[Triangles[i].vertex[0]].x;
                vArray[j + 6] = vertex[Triangles[i].vertex[0]].y;
                vArray[j + 7] = vertex[Triangles[i].vertex[0]].z;

                vArray[j + 10] = normals[Triangles[i].vertex[1]].x;
                vArray[j + 11] = normals[Triangles[i].vertex[1]].y;
                vArray[j + 12] = normals[Triangles[i].vertex[1]].z;
                vArray[j + 13] = vertex[Triangles[i].vertex[1]].x;
                vArray[j + 14] = vertex[Triangles[i].vertex[1]].y;
                vArray[j + 15] = vertex[Triangles[i].vertex[1]].z;

                vArray[j + 18] = normals[Triangles[i].vertex[2]].x;
                vArray[j + 19] = normals[Triangles[i].vertex[2]].y;
                vArray[j + 20] = normals[Triangles[i].vertex[2]].z;
                vArray[j + 21] = vertex[Triangles[i].vertex[2]].x;
                vArray[j + 22] = vertex[Triangles[i].vertex[2]].y;
                vArray[j + 23] = vertex[Triangles[i].vertex[2]].z;
            }
        }
    }

    void update_notexnonorm(std::vector<TexturedTriangle> &Triangles, XYZ *vertex, GLfloat *vArray){
        MICROPROFILE_SCOPEI("Model", "update_notexnonorm", 0xe4a77c4);
        if (model->type != normaltype && model->type != decalstype) {
            return;
        }
        for (unsigned int i = start_idx; i <= end_idx; i++) {
            unsigned int j = i * 24;
            vArray[j + 5] = vertex[Triangles[i].vertex[0]].x;
            vArray[j + 6] = vertex[Triangles[i].vertex[0]].y;
            vArray[j + 7] = vertex[Triangles[i].vertex[0]].z;

            vArray[j + 13] = vertex[Triangles[i].vertex[1]].x;
            vArray[j + 14] = vertex[Triangles[i].vertex[1]].y;
            vArray[j + 15] = vertex[Triangles[i].vertex[1]].z;

            vArray[j + 21] = vertex[Triangles[i].vertex[2]].x;
            vArray[j + 22] = vertex[Triangles[i].vertex[2]].y;
            vArray[j + 23] = vertex[Triangles[i].vertex[2]].z;
        }
    }
    void execute() override {
        switch(type){
            default:
                update_notex(model->Triangles, model->vertex, model->normals, model->vArray);
                break;
            case 1:
                update_notexnonorm(model->Triangles, model->vertex, model->vArray);
                break;
        }
    }
};

void Model::submitCalculateNormalsJobs_Phase1(
    bool facenormalise,
    WorkerThread::JobHandle dep,
    std::vector<WorkerThread::JobHandle> &out
){
    for (int i = 0; i < vertexNum; i++) {
        normals[i].x = 0;
        normals[i].y = 0;
        normals[i].z = 0;
    }

    //Phase 1 (calculate normals)
    size_t numtris = Triangles.size();
    size_t p1_job_size = numtris / CALCNORM_JOB_SPLIT_DENOM;
    if(p1_job_size == 0){
        p1_job_size = 1;
    }
    for (size_t i = 0; i < numtris; i+=p1_job_size) {
        size_t end = i + p1_job_size - 1;
        if(end >= numtris){
            end = numtris - 1;
        }
        if(dep >= 0){
            out.push_back(WorkerThread::submitDependentJob<CalculateNormalsJob>(dep, i, end, this, facenormalise));
        }else{
            out.push_back(WorkerThread::submitJob<CalculateNormalsJob>(i, end, this, facenormalise));
        }
    }
}

void Model::submitCalculateNormalsJobs_Phase2(std::vector<WorkerThread::JobHandle> &out){
    //Phase 2 (normalize verts)
    size_t p2_job_size = vertexNum / NORM_VERTS_JOB_SPLIT_DENOM;
    if(p2_job_size == 0){
        p2_job_size = 1;
    }
    for (int i = 0; i < vertexNum; i+=p2_job_size) {
        int end = i + p2_job_size - 1;
        if(end >= vertexNum){
            end = vertexNum - 1;
        }
        out.push_back(WorkerThread::submitJob<NormalizeVertsJob>(i, end, this));
    }
}

void Model::submitCalculateNormalsJobs_Phase3(int type, std::vector<WorkerThread::JobHandle> &out){
    size_t numtris = Triangles.size();
    size_t job_size = numtris / UPDATE_VERT_JOB_SPLIT_DENOM;
    if(job_size == 0){
        job_size = 1;
    }
    for (size_t i = 0; i < numtris; i+=job_size) {
        size_t end = i + job_size - 1;
        if(end >= numtris){
            end = numtris - 1;
        }

        out.push_back(WorkerThread::submitJob<UpdateVertexJob>(type, i, end, this));
    }
}

void Model::CalculateNormals(bool facenormalise)
{
    MICROPROFILE_SCOPEI("Model", "CalculateNormals", 0x008fff);
    //Game::LoadingScreen();

    if (type != normaltype && type != decalstype) {
        return;
    }

    for (int i = 0; i < vertexNum; i++) {
        normals[i].x = 0;
        normals[i].y = 0;
        normals[i].z = 0;
    }

    for (size_t i = 0; i < Triangles.size(); i++) {
        /*
        CrossProduct(
            vertex[Triangles[i].vertex[1]] - vertex[Triangles[i].vertex[0]],
            vertex[Triangles[i].vertex[2]] - vertex[Triangles[i].vertex[0]],
            &Triangles[i].facenormal
        );
        */
        XYZ v0 = vertex[Triangles[i].vertex[1]] - vertex[Triangles[i].vertex[0]];
        XYZ v1 = vertex[Triangles[i].vertex[2]] - vertex[Triangles[i].vertex[0]];
        cross3_neon(&v0.x, &v1.x, &Triangles[i].facenormal.x);

        normals[Triangles[i].vertex[0]].x += Triangles[i].facenormal.x;
        normals[Triangles[i].vertex[0]].y += Triangles[i].facenormal.y;
        normals[Triangles[i].vertex[0]].z += Triangles[i].facenormal.z;

        normals[Triangles[i].vertex[1]].x += Triangles[i].facenormal.x;
        normals[Triangles[i].vertex[1]].y += Triangles[i].facenormal.y;
        normals[Triangles[i].vertex[1]].z += Triangles[i].facenormal.z;

        normals[Triangles[i].vertex[2]].x += Triangles[i].facenormal.x;
        normals[Triangles[i].vertex[2]].y += Triangles[i].facenormal.y;
        normals[Triangles[i].vertex[2]].z += Triangles[i].facenormal.z;
        if (facenormalise) {
            Normalise(&Triangles[i].facenormal);
        }
    }

    for (int i = 0; i < vertexNum; i++) {
        Normalise(&normals[i]);
        normals[i] *= -1;
    }

    UpdateVertexArrayNoTex();
}

void Model::drawimmediate()
{
    MICROPROFILE_SCOPEI("Model", "drawimmediate", 0x008fff);
    textureptr.bind();
    glBegin(GL_TRIANGLES);
    for (unsigned int i = 0; i < Triangles.size(); i++) {
        glTexCoord2f(Triangles[i].gx[0], Triangles[i].gy[0]);
        if (color) {
            glColor3f(normals[Triangles[i].vertex[0]].x, normals[Triangles[i].vertex[0]].y, normals[Triangles[i].vertex[0]].z);
        } else if (flat) {
            glNormal3f(Triangles[i].facenormal.x, Triangles[i].facenormal.y, Triangles[i].facenormal.y);
        } else {
            glNormal3f(normals[Triangles[i].vertex[0]].x, normals[Triangles[i].vertex[0]].y, normals[Triangles[i].vertex[0]].z);
        }
        glVertex3f(vertex[Triangles[i].vertex[0]].x, vertex[Triangles[i].vertex[0]].y, vertex[Triangles[i].vertex[0]].z);

        glTexCoord2f(Triangles[i].gx[1], Triangles[i].gy[1]);
        if (color) {
            glColor3f(normals[Triangles[i].vertex[1]].x, normals[Triangles[i].vertex[1]].y, normals[Triangles[i].vertex[1]].z);
        } else if (flat) {
            glNormal3f(Triangles[i].facenormal.x, Triangles[i].facenormal.y, Triangles[i].facenormal.y);
        } else {
            glNormal3f(normals[Triangles[i].vertex[1]].x, normals[Triangles[i].vertex[1]].y, normals[Triangles[i].vertex[1]].z);
        }
        glVertex3f(vertex[Triangles[i].vertex[1]].x, vertex[Triangles[i].vertex[1]].y, vertex[Triangles[i].vertex[1]].z);

        glTexCoord2f(Triangles[i].gx[2], Triangles[i].gy[2]);
        if (color) {
            glColor3f(normals[Triangles[i].vertex[2]].x, normals[Triangles[i].vertex[2]].y, normals[Triangles[i].vertex[2]].z);
        } else if (flat) {
            glNormal3f(Triangles[i].facenormal.x, Triangles[i].facenormal.y, Triangles[i].facenormal.y);
        } else {
            glNormal3f(normals[Triangles[i].vertex[2]].x, normals[Triangles[i].vertex[2]].y, normals[Triangles[i].vertex[2]].z);
        }
        glVertex3f(vertex[Triangles[i].vertex[2]].x, vertex[Triangles[i].vertex[2]].y, vertex[Triangles[i].vertex[2]].z);
    }
    glEnd();
}

void Model::draw()
{
    MICROPROFILE_SCOPEI("Model", "draw", 0x008fff);

    if (type != normaltype && type != decalstype) {
        return;
    }

    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    
    if (color) {
        glInterleavedArrays(GL_T2F_C3F_V3F, 8 * sizeof(GLfloat), &vArray[0]);
    } else {
        glInterleavedArrays(GL_T2F_N3F_V3F, 8 * sizeof(GLfloat), &vArray[0]);
    }
    textureptr.bind();

    glDrawArrays(GL_TRIANGLES, 0, Triangles.size() * 3);

    if (color) {
        glDisableClientState(GL_COLOR_ARRAY);
    } else {
        glDisableClientState(GL_NORMAL_ARRAY);
    }
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

void Model::drawdifftex(Texture texture)
{
    MICROPROFILE_SCOPEI("Model", "drawdifftex", 0x008fff);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    if (color) {
        glInterleavedArrays(GL_T2F_C3F_V3F, 8 * sizeof(GLfloat), &vArray[0]);
    } else {
        glInterleavedArrays(GL_T2F_N3F_V3F, 8 * sizeof(GLfloat), &vArray[0]);
    }

    texture.bind();
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glDrawArrays(GL_TRIANGLES, 0, Triangles.size() * 3);

    if (color) {
        glDisableClientState(GL_COLOR_ARRAY);
    } else {
        glDisableClientState(GL_NORMAL_ARRAY);
    }
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

void Model::drawdecals(Texture shadowtexture, Texture bloodtexture, Texture bloodtexture2, Texture breaktexture)
{
    MICROPROFILE_SCOPEI("Model", "drawdecals", 0x008fff);
    if (decalstoggle) {
        if (type != decalstype) {
            return;
        }

        bool blend = true;
        int lasttype = -1;

        glEnable(GL_BLEND);
        glDisable(GL_LIGHTING);
        glDisable(GL_CULL_FACE);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(0);
        for (unsigned int i = 0; i < decals.size(); i++) {
            if (decals[i].type == blooddecalfast && decals[i].alivetime < 2) {
                decals[i].alivetime = 2;
            }

            if (decals[i].type != lasttype) {
                if (decals[i].type == shadowdecal) {
                    shadowtexture.bind();
                    if (!blend) {
                        blend = 1;
                        glAlphaFunc(GL_GREATER, 0.0001);
                        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    }
                }
                if (decals[i].type == breakdecal) {
                    breaktexture.bind();
                    if (!blend) {
                        blend = 1;
                        glAlphaFunc(GL_GREATER, 0.0001);
                        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    }
                }
                if (decals[i].type == blooddecal || decals[i].type == blooddecalslow) {
                    bloodtexture.bind();
                    if (blend) {
                        blend = 0;
                        glAlphaFunc(GL_GREATER, 0.15);
                        glBlendFunc(GL_ONE, GL_ZERO);
                    }
                }
                if (decals[i].type == blooddecalfast) {
                    bloodtexture2.bind();
                    if (blend) {
                        blend = 0;
                        glAlphaFunc(GL_GREATER, 0.15);
                        glBlendFunc(GL_ONE, GL_ZERO);
                    }
                }
            }
            if (decals[i].type == shadowdecal) {
                glColor4f(1, 1, 1, decals[i].opacity);
            }
            if (decals[i].type == breakdecal) {
                glColor4f(1, 1, 1, decals[i].opacity);
                if (decals[i].alivetime > 58) {
                    glColor4f(1, 1, 1, decals[i].opacity * (60 - decals[i].alivetime) / 2);
                }
            }
            if ((decals[i].type == blooddecal || decals[i].type == blooddecalfast || decals[i].type == blooddecalslow)) {
                glColor4f(1, 1, 1, decals[i].opacity);
                if (decals[i].alivetime < 4) {
                    glColor4f(1, 1, 1, decals[i].opacity * decals[i].alivetime * .25);
                }
                if (decals[i].alivetime > 58) {
                    glColor4f(1, 1, 1, decals[i].opacity * (60 - decals[i].alivetime) / 2);
                }
            }
            lasttype = decals[i].type;
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glBegin(GL_TRIANGLES);
            for (int j = 0; j < 3; j++) {
                glTexCoord2f(decals[i].texcoords[j][0], decals[i].texcoords[j][1]);
                glVertex3f(decals[i].vertex[j].x, decals[i].vertex[j].y, decals[i].vertex[j].z);
            }
            glEnd();
            glPopMatrix();
        }
        for (int i = decals.size() - 1; i >= 0; i--) {
            decals[i].alivetime += multiplier;
            if (decals[i].type == blooddecalslow) {
                decals[i].alivetime -= multiplier * 2 / 3;
            }
            if (decals[i].type == blooddecalfast) {
                decals[i].alivetime += multiplier * 4;
            }
            if (decals[i].type == shadowdecal) {
                DeleteDecal(i);
            }
            if ((decals[i].type == blooddecal || decals[i].type == blooddecalfast || decals[i].type == blooddecalslow) && decals[i].alivetime >= 60) {
                DeleteDecal(i);
            }
        }
        glAlphaFunc(GL_GREATER, 0.0001);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
}

void Model::DeleteDecal(int which)
{
    MICROPROFILE_SCOPEI("Model", "DeleteDecal", 0x008fff);
    if (decalstoggle) {
        if (type != decalstype) {
            return;
        }
        decals.erase(decals.begin() + which);
    }
}

void Model::MakeDecal(decal_type atype, XYZ* where, float* size, float* opacity, float* rotation)
{
    MICROPROFILE_SCOPEI("Model", "MakeDecal", 0x008fff);
    if (decalstoggle) {
        if (type != decalstype) {
            return;
        }

        XYZ rot;
        float distance;

        if (*opacity > 0) {
            if (distsq(where, &boundingspherecenter) < (boundingsphereradius + *size) * (boundingsphereradius + *size)) {
                for (unsigned int i = 0; i < Triangles.size(); i++) {
                    if (Triangles[i].facenormal.y < -.1 && (vertex[Triangles[i].vertex[0]].y < where->y || vertex[Triangles[i].vertex[1]].y < where->y || vertex[Triangles[i].vertex[2]].y < where->y)) {
                        distance = abs(((Triangles[i].facenormal.x * where->x) + (Triangles[i].facenormal.y * where->y) + (Triangles[i].facenormal.z * where->z) - ((Triangles[i].facenormal.x * vertex[Triangles[i].vertex[0]].x) + (Triangles[i].facenormal.y * vertex[Triangles[i].vertex[0]].y) + (Triangles[i].facenormal.z * vertex[Triangles[i].vertex[0]].z))) / Triangles[i].facenormal.y);

                        if ((*opacity - distance / 10) > 0) {
                            Decal decal(*where, atype, *opacity - distance / 10, *rotation, *size, *this, i, 0);

                            if (!(decal.texcoords[0][0] < 0 && decal.texcoords[1][0] < 0 && decal.texcoords[2][0] < 0)) {
                                if (!(decal.texcoords[0][1] < 0 && decal.texcoords[1][1] < 0 && decal.texcoords[2][1] < 0)) {
                                    if (!(decal.texcoords[0][0] > 1 && decal.texcoords[1][0] > 1 && decal.texcoords[2][0] > 1)) {
                                        if (!(decal.texcoords[0][1] > 1 && decal.texcoords[1][1] > 1 && decal.texcoords[2][1] > 1)) {
                                            if (decal.rotation) {
                                                for (int j = 0; j < 3; j++) {
                                                    rot.y = 0;
                                                    rot.x = decal.texcoords[j][0] - .5;
                                                    rot.z = decal.texcoords[j][1] - .5;
                                                    rot = DoRotation(rot, 0, -decal.rotation, 0);
                                                    decal.texcoords[j][0] = rot.x + .5;
                                                    decal.texcoords[j][1] = rot.z + .5;
                                                }
                                            }
                                            if (decals.size() < max_model_decals - 1) {
                                                decals.push_back(decal);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void Model::MakeDecal(decal_type atype, XYZ where, float size, float opacity, float rotation)
{
    MICROPROFILE_SCOPEI("Model", "MakeDecal", 0x008fff);
    if (decalstoggle) {
        if (type != decalstype) {
            return;
        }

        XYZ rot;
        float distance;

        if (opacity > 0) {
            if (distsq(&where, &boundingspherecenter) < (boundingsphereradius + size) * (boundingsphereradius + size)) {
                for (unsigned int i = 0; i < Triangles.size(); i++) {
                    distance = abs(((Triangles[i].facenormal.x * where.x) + (Triangles[i].facenormal.y * where.y) + (Triangles[i].facenormal.z * where.z) - ((Triangles[i].facenormal.x * vertex[Triangles[i].vertex[0]].x) + (Triangles[i].facenormal.y * vertex[Triangles[i].vertex[0]].y) + (Triangles[i].facenormal.z * vertex[Triangles[i].vertex[0]].z))));
                    if (distance < .02 && abs(Triangles[i].facenormal.y) > abs(Triangles[i].facenormal.x) && abs(Triangles[i].facenormal.y) > abs(Triangles[i].facenormal.z)) {
                        if ((opacity - distance / 10) > 0) {
                            Decal decal(where, atype, opacity - distance / 10, rotation, size, *this, i, 0);

                            if (!(decal.texcoords[0][0] < 0 && decal.texcoords[1][0] < 0 && decal.texcoords[2][0] < 0)) {
                                if (!(decal.texcoords[0][1] < 0 && decal.texcoords[1][1] < 0 && decal.texcoords[2][1] < 0)) {
                                    if (!(decal.texcoords[0][0] > 1 && decal.texcoords[1][0] > 1 && decal.texcoords[2][0] > 1)) {
                                        if (!(decal.texcoords[0][1] > 1 && decal.texcoords[1][1] > 1 && decal.texcoords[2][1] > 1)) {
                                            if (decal.rotation) {
                                                for (int j = 0; j < 3; j++) {
                                                    rot.y = 0;
                                                    rot.x = decal.texcoords[j][0] - .5;
                                                    rot.z = decal.texcoords[j][1] - .5;
                                                    rot = DoRotation(rot, 0, -decal.rotation, 0);
                                                    decal.texcoords[j][0] = rot.x + .5;
                                                    decal.texcoords[j][1] = rot.z + .5;
                                                }
                                            }
                                            if (decals.size() < max_model_decals - 1) {
                                                decals.push_back(decal);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    } else if (distance < .02 && abs(Triangles[i].facenormal.x) > abs(Triangles[i].facenormal.y) && abs(Triangles[i].facenormal.x) > abs(Triangles[i].facenormal.z)) {
                        if ((opacity - distance / 10) > 0) {
                            Decal decal(where, atype, opacity - distance / 10, rotation, size, *this, i, 1);

                            if (!(decal.texcoords[0][0] < 0 && decal.texcoords[1][0] < 0 && decal.texcoords[2][0] < 0)) {
                                if (!(decal.texcoords[0][1] < 0 && decal.texcoords[1][1] < 0 && decal.texcoords[2][1] < 0)) {
                                    if (!(decal.texcoords[0][0] > 1 && decal.texcoords[1][0] > 1 && decal.texcoords[2][0] > 1)) {
                                        if (!(decal.texcoords[0][1] > 1 && decal.texcoords[1][1] > 1 && decal.texcoords[2][1] > 1)) {
                                            if (decal.rotation) {
                                                for (int j = 0; j < 3; j++) {
                                                    rot.y = 0;
                                                    rot.x = decal.texcoords[j][0] - .5;
                                                    rot.z = decal.texcoords[j][1] - .5;
                                                    rot = DoRotation(rot, 0, -decal.rotation, 0);
                                                    decal.texcoords[j][0] = rot.x + .5;
                                                    decal.texcoords[j][1] = rot.z + .5;
                                                }
                                            }
                                            if (decals.size() < max_model_decals - 1) {
                                                decals.push_back(decal);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    } else if (distance < .02 && abs(Triangles[i].facenormal.z) > abs(Triangles[i].facenormal.y) && abs(Triangles[i].facenormal.z) > abs(Triangles[i].facenormal.x)) {
                        if ((opacity - distance / 10) > 0) {
                            Decal decal(where, atype, opacity - distance / 10, rotation, size, *this, i, 2);

                            if (!(decal.texcoords[0][0] < 0 && decal.texcoords[1][0] < 0 && decal.texcoords[2][0] < 0)) {
                                if (!(decal.texcoords[0][1] < 0 && decal.texcoords[1][1] < 0 && decal.texcoords[2][1] < 0)) {
                                    if (!(decal.texcoords[0][0] > 1 && decal.texcoords[1][0] > 1 && decal.texcoords[2][0] > 1)) {
                                        if (!(decal.texcoords[0][1] > 1 && decal.texcoords[1][1] > 1 && decal.texcoords[2][1] > 1)) {
                                            if (decal.rotation) {
                                                for (int j = 0; j < 3; j++) {
                                                    rot.y = 0;
                                                    rot.x = decal.texcoords[j][0] - .5;
                                                    rot.z = decal.texcoords[j][1] - .5;
                                                    rot = DoRotation(rot, 0, -decal.rotation, 0);
                                                    decal.texcoords[j][0] = rot.x + .5;
                                                    decal.texcoords[j][1] = rot.z + .5;
                                                }
                                            }
                                            if (decals.size() < max_model_decals - 1) {
                                                decals.push_back(decal);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

const XYZ& Model::getTriangleVertex(unsigned triangleId, unsigned vertexId) const
{
    return vertex[Triangles[triangleId].vertex[vertexId]];
}

void Model::deleteDeadDecals()
{
    MICROPROFILE_SCOPEI("Model", "deleteDeadDecals", 0x008fff);
    for (int i = decals.size() - 1; i >= 0; i--) {
        if ((decals[i].type == blooddecal || decals[i].type == blooddecalslow) && decals[i].alivetime < 2) {
            DeleteDecal(i);
        }
    }
}

Model::~Model()
{
    deallocate();
}

void Model::deallocate()
{
    if (owner) {
        free(owner);
    }
    owner = 0;

    if (vertex) {
        free(vertex);
    }
    vertex = 0;

    if (normals) {
        free(normals);
    }
    normals = 0;

    if (vArray) {
        #ifdef DRAW_SPEEDHACK
        if(vgl_array){
            vgl_free(vArray);
        }else{
            free(vArray);
        }
        #else
            free(vArray);
        #endif
    }
    vArray = 0;

    decals.clear();
}

Model::Model()
    : vertexNum(0)
    , type(nothing)
    , owner(0)
    , vertex(0)
    , normals(0)
    , vArray(0)
    , vgl_array(false)
    , color(0)
    , boundingspherecenter()
    , boundingsphereradius(0)
    , flat(false)
{
    memset(&modelTexture, 0, sizeof(modelTexture));
}
