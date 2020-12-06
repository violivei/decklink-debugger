#include "util.h"
#include "log.h"

const char* to_cstr(std::string && s)
{
    static thread_local std::string sloc;
    sloc = std::move(s);
    return sloc.c_str();
}

void throwIfNotOk(HRESULT result, const char* message)
{
	if(result != S_OK) {
		LOG(ERROR) << "result (=0x" << std::hex << result << ") != S_OK, " << message;
		throw message;
	}
}

void throwIfNull(void* ptr, const char* message)
{
	if(ptr == nullptr) {
		LOG(ERROR) << "ptr == nullptr, " << message;
		throw message;
	}
}

