#pragma once
#ifndef  _MSC_VER

#include <new>
#include <cstdlib>

void UnixInit();

void UnixGetSymbols(void* caller, char* fn, int* ln);

void* UnixPageAllignedAlloc(size_t size);

void UnixPageAllignedFree(void* p, size_t size);

void UnixPageRelease(void* p, size_t size);


// Global overload 
void* operator new(size_t size);
void* operator new(size_t size, const std::nothrow_t&) noexcept;
void* operator new[](size_t size);
void* operator new[](size_t size, const std::nothrow_t&) noexcept;
void operator delete(void* address) noexcept;
void operator delete(void* address, size_t size) noexcept;
void operator delete(void* address, const std::nothrow_t&) noexcept;
void operator delete[](void* address) noexcept;
void operator delete[](void* address, size_t size) noexcept;
void operator delete[](void* address, const std::nothrow_t&) noexcept;

// tagged allocation
void* operator new(size_t size, const char* tag);
void* operator new(size_t size, const std::nothrow_t&, const char* tag) noexcept;
void* operator new[](size_t size, const char* tag);
void* operator new[](size_t size, const std::nothrow_t&, const char* tag) noexcept;

#endif