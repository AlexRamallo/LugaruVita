#ifndef __FILE_CACHE__H__
#define __FILE_CACHE__H__
#include <stdio.h>
#include <string>
/**
 * Simple in-memory cache to prevent loading a file more than once
 * */
namespace FileCache {
	//if no cache is initialized, falls back to fopen(..., "rb")
	FILE *readFile(const std::string &filename);

	void InitCache();
	void ClearCache();
};
#endif //__FILE_CACHE__H__