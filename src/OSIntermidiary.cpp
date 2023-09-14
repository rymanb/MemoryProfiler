#include "OSIntermidiary.h"
#include "Common.h"




void Initialize()
{
#ifdef _MSC_VER
	WinInit();
#else
	UnixInit();
#endif // _MSC_VER
}

void GetSymbols(void* caller, char* fn, int* ln)
{
#ifdef _MSC_VER
	WinGetSymbols(caller, fn, ln);
#else
	UnixGetSymbols(caller, fn, ln);
#endif // _MSC_VER
}

void* PageAllignedAlloc(size_t size)
{
#ifdef _MSC_VER
	return WinPageAllignedAlloc(size);
#else
	return UnixPageAllignedAlloc(size);
#endif // _MSC_VER


}

void PageAllignedFree(void* p, size_t size)
{
#ifdef _MSC_VER
	size = size;
	WinPageAllignedFree(p);
#else
	UnixPageAllignedFree(p, size);
#endif // _MSC_VER
}

void PageRelease(void* p, size_t size)
{
#ifdef _MSC_VER
	size = size;
	WinPageRelease(p);
#else
	UnixPageRelease(p, size);
#endif // _MSC_VER
}
