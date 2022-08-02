#include "Utils/FileCache.hpp"
#include "Utils/Log.h"
#include <stdio.h>
#include <map>

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

static std::map<std::string, CacheEntry> entries;
static bool cache_active = false;

static FILE *getCache(const std::string &filename){
	CacheEntry &entry = entries[filename];
	return entry.handle(filename);
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
}

void ClearCache() {
	cache_active = false;
	entries.clear();
}

}; //namespace FileCache