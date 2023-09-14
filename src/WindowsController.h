
#pragma once
#ifdef  _MSC_VER

#include <new>

void WinInit();

void WinGetSymbols(void* caller, char* fn, int* ln);

void* WinPageAllignedAlloc(size_t size);

void WinPageAllignedFree(void* p);

void WinPageRelease(void* p);

// For MSVC
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


#endif // _MSC_VER
