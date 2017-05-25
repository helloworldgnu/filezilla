#include "stdafx.h"
#include "dll.h"

DLL::DLL()
{
}

DLL::DLL(CString const& s)
{
	load(s);
}

DLL::~DLL()
{
	clear();
}

bool DLL::load(CString const& s)
{
	clear();
	hModule = LoadLibraryEx(s, 0, LOAD_LIBRARY_SEARCH_APPLICATION_DIR);
	return hModule != 0;
}

void DLL::clear()
{
	if (hModule) {
		for( auto & f : loaded_functions ) {
			*f = 0;
		}
		FreeLibrary(hModule);
		hModule = 0;
	}
}

void* DLL::load_func(LPCSTR name, void** out)
{
	void* ret = 0;
	if (hModule) {
		ret = GetProcAddress(hModule, name);
	}

	if (out) {
		*out = ret;
		if (ret) {
			loaded_functions.push_back(out);
		}
	}

	return ret;
}
