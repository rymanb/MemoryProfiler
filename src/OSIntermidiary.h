#pragma once
#include <cstddef>

#ifdef _MSC_VER
#include "WindowsController.h"
#else
#include "UnixController.h"
#endif // _MSC_VER

void Initialize(); // initializes the memory debugger

void GetSymbols(void* caller, char* fn, int* ln); // gets the symbols of the caller

void* PageAllignedAlloc(size_t size); // allocates a page alligned memory block

void PageAllignedFree(void* p, size_t size); // frees a page alligned memory block

void PageRelease(void* p, size_t size); // releases a page alligned memory block
