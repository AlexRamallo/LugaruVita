#ifndef _LOG_H_
#define _LOG_H_
#include <string>

#define LOGFUNC

void LOG_TOGGLE(char set);
void LOG(const std::string& fmt, ...);

#ifdef NDEBUG
	#define ASSERT(cond)
#else
	//helps ensure vita-parse-core displays `cond` when reading a core dump
	#define ASSERT(cond) {if(!(cond)){__attribute__((unused)) int x = *((int*)nullptr);x=0;}}
#endif

#endif//__LOG_H__