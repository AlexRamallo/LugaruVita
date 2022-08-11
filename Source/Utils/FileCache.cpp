#include "Utils/FileCache.hpp"
#include "Utils/Log.h"
#include <pthread.h>
#include <stdio.h>
#include <map>

#if PACK_ASSETS
	#include <physfs.h>
#endif

namespace FileCache {

struct CacheEntry {
	void *data;
	size_t size;
	std::string filename;

	CacheEntry():
		data(nullptr),
		size(0),
		filename()
	{
		//--
	}

	~CacheEntry()
	{
		if(data != nullptr){
			free(data);
			data = nullptr;
			size = 0;
		}
	}

	FILE *handle(const std::string &fname){
		if(data == nullptr){
			filename = fname;
			FILE *f = fopen(filename.c_str(), "rb");
			if(f == nullptr){
				return nullptr;
			}

			fseek(f, 0L, SEEK_END);
			size = ftell(f);
			rewind(f);

			data = malloc(size);

			fread(data, size, 1, f);
			fclose(f);
		}
        return fmemopen(data, size, "rb");
	}
};

static pthread_mutex_t mtxCache;
static std::map<std::string, CacheEntry> entries;
static bool cache_active = false;
static bool did_init_once = false;
static FILE *getCache(const std::string &filename){
	if(pthread_mutex_lock(&mtxCache)){
		ASSERT(!"Failed to lock file cache mutex");
		return nullptr;
	}

	CacheEntry &entry = entries[filename];
	FILE *ret = entry.handle(filename);

	if(pthread_mutex_unlock(&mtxCache)){
		ASSERT(!"Failed to unlock file cache mutex");
		return nullptr;
	}
	return ret;
}

FILE *readFile(const std::string &filename) {
	if(!cache_active){
		return fopen(filename.c_str(), "rb");
	}else{
		return getCache(filename);
	}
}

void InitCache() {
	cache_active = true;
	if(!did_init_once){
		did_init_once = true;
		if(pthread_mutex_init(&mtxCache, NULL)){
			ASSERT(!"Failed to init file cache mutex!");
		}
	}
}

void ClearCache() {
	cache_active = false;
	entries.clear();
}

}; //namespace FileCache