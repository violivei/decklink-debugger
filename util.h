#ifndef __util__
#define __util__

#include <string>
#include "DeckLinkAPI.h"

#define UNUSED __attribute__ ((unused))

const char* to_cstr(std::string && s);
void throwIfNotOk(HRESULT result, const char* message);
void throwIfNull(void* ptr, const char* message);
#endif
