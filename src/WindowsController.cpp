#ifdef  _MSC_VER

#include "Common.h"
#include <Windows.h>
#include <DbgHelp.h>
#include <new>
#include "WindowsController.h"
#include "MemoryDebugger.h"

#pragma comment ( lib, "dbghelp.lib" )

void WinInit()
{
	// Initialize the symbol handler
	SymInitialize(GetCurrentProcess(), NULL, true);
	SymSetOptions(SymGetOptions() | SYMOPT_LOAD_LINES);
}

void WinGetSymbols(void* caller, char* fn, int* ln)
{
	if (caller == nullptr)
	{
		return;
	}
	
	// Get the symbol
	SymSetOptions(SYMOPT_LOAD_LINES);
	DWORD  dwDisplacement;
	IMAGEHLP_LINE64 line;
	line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

	SymGetLineFromAddr64(GetCurrentProcess(), (DWORD64)caller, &dwDisplacement, &line);

	*ln = line.LineNumber;
	if (line.FileName != nullptr)
	{
		strcpy_s(fn, 256, line.FileName);
	}
	else
	{
		strcpy_s(fn, 256, "Unknown");
	}
}

void* WinPageAllignedAlloc(size_t size)
{
	// calculate page size
	size_t pageSize = MIN_PAGE_SIZE;
	if (size > MIN_PAGE_SIZE)
	{
		pageSize = size + MIN_PAGE_SIZE - (size % MIN_PAGE_SIZE);
	}
	// allocate pages
	void* p = VirtualAlloc(0, pageSize + MIN_PAGE_SIZE, MEM_RESERVE, PAGE_NOACCESS);
	if (p)
	{
		// commit all but the last page
		p = VirtualAlloc(p, pageSize, MEM_COMMIT, PAGE_READWRITE);

		// get pointer to start of object
		unsigned char* obj = (unsigned char*)p + (pageSize - size);

		// mark no man's land with 0xfdfdfdfd
#ifdef _DEBUG
		if (p)
		{
			memset(p, 0xfd, pageSize - size);

			// mark uninitialized memory with 0xcdcdcdcd
			memset(obj, 0xcd, size);
		}
#endif // _DEBUG

		return obj; // return pointer to start of object
	}
	return nullptr;
}

void WinPageAllignedFree(void* p)
{
	if (p == nullptr)
	{
		return;
	}
	
	// decommit all pages for this object
	VirtualFree(p, 0, MEM_DECOMMIT);
}

void WinPageRelease(void* p)
{
	if (p == nullptr)
	{
		return;
	}

	// release all pages for this object
	VirtualFree(p, 0, MEM_RELEASE);
}



void* operator new(size_t size)
{
	return MemoryDebugger::GetInstance().Allocate(size, MemoryDebugger::CreateHeader(), _ReturnAddress());
}
void* operator new(size_t size, const std::nothrow_t&) noexcept
{
	return MemoryDebugger::GetInstance().Allocate(size, MemoryDebugger::CreateHeader(), _ReturnAddress(), false);
}
void* operator new[](size_t size)
{
	return MemoryDebugger::GetInstance().Allocate(size, MemoryDebugger::CreateHeader(true), _ReturnAddress());
}
void* operator new[](size_t size, const std::nothrow_t&) noexcept
{
	return MemoryDebugger::GetInstance().Allocate(size, MemoryDebugger::CreateHeader(true), _ReturnAddress(), false);
}


void operator delete(void* address) noexcept
{
	MemoryDebugger::GetInstance().Deallocate(address, _ReturnAddress(), false, false);
}
void operator delete(void* address, size_t size) noexcept
{
	size = size;
	MemoryDebugger::GetInstance().Deallocate(address, _ReturnAddress(), false, false);
}
void operator delete(void* address, const std::nothrow_t&) noexcept
{
	MemoryDebugger::GetInstance().Deallocate(address, _ReturnAddress(), true, false);
}
void operator delete[](void* address) noexcept
{
	MemoryDebugger::GetInstance().Deallocate(address, _ReturnAddress(), false, true);
}
void operator delete[](void* address, size_t size) noexcept
{
	size = size;
	MemoryDebugger::GetInstance().Deallocate(address, _ReturnAddress(), false, true);
}
void operator delete[](void* address, const std::nothrow_t&) noexcept
{
	MemoryDebugger::GetInstance().Deallocate(address, _ReturnAddress(), true, true);
}


void* operator new(size_t size, const char* tag)
{
	return MemoryDebugger::GetInstance().Allocate(size, MemoryDebugger::CreateHeader(), _ReturnAddress(), true, tag);
}
void* operator new(size_t size, const std::nothrow_t&, const char* tag) noexcept
{
	return MemoryDebugger::GetInstance().Allocate(size, MemoryDebugger::CreateHeader(), _ReturnAddress(), false, tag);
}
void* operator new[](size_t size, const char* tag)
{
	return MemoryDebugger::GetInstance().Allocate(size, MemoryDebugger::CreateHeader(true), _ReturnAddress(), true, tag);
}
void* operator new[](size_t size, const std::nothrow_t&, const char* tag) noexcept
{
	return MemoryDebugger::GetInstance().Allocate(size, MemoryDebugger::CreateHeader(true), _ReturnAddress(), false, tag);
}


#endif