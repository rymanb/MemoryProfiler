#ifndef _MSC_VER

#include "Common.h"
#include "UnixController.h"
#include "MemoryDebugger.h"
#include <sys/mman.h> // for mmap/munmap
#include <execinfo.h> // for backtrace (debug info)
#include <dlfcn.h>
#include <link.h>
#include <iostream>

#define MAX_BUFFER_SIZE 1024

// no need to initialize anything on Unix
void UnixInit()
{
}

// get the path of the executable
std::basic_string<char, std::char_traits<char>, Mallocator<char>> get_path(size_t j)
{
	void *array[MAX_BUFFER_SIZE];
	size_t size;
	char **strings;

	size = backtrace(array, MAX_BUFFER_SIZE);
	strings = backtrace_symbols(array, size);

	if (j >= size)
		j = size - 1;

	std::basic_string<char, std::char_traits<char>, Mallocator<char>> result = strings[j];

	free(strings);

	return result;
}

// gets address of caller
size_t ConvertToVMA(size_t addr)
{
	Dl_info info;
	link_map *link_map;
	dladdr1((void *)addr, &info, (void **)&link_map, RTLD_DL_LINKMAP);
	return addr - link_map->l_addr;
}

struct DebugInfo
{
	int line;
	Basic_String file;
};

// get the file and line number of the caller
void UnixGetSymbols(void *caller, char *fn, int *ln)
{
	if (caller == nullptr)
	{
		return;
	}

	// turn the address into a string
	Basic_String traceInfo = get_path(0);

	int p = 0;
	Basic_String sysstr = "/bin/addr2line -e ";
	while (traceInfo[p] != '(' && traceInfo[p] != ' ' && traceInfo[p] != 0)
	{
		sysstr += traceInfo[p];
		++p;
	}

	sysstr += " ";

	size_t vma_address = ConvertToVMA((size_t)caller);
	vma_address -= 1;

	// a 64 bit address cant possibly be longer than 100 characters
	const size_t max_address_size_64 = 100;
	char buf[max_address_size_64];
	sprintf(buf, "%zx", vma_address);

	sysstr += buf;

	FILE *fp = popen(sysstr.c_str(), "r");
	if (!fp)
	{
		std::cout << "ERROR: unable to open pipe.";
		return;
	}

	Basic_String info;
	DebugInfo debugInfo;
	char path[1035] = {0};

	/* Read the output a line at a time. */
	while (fgets(path, sizeof(path), fp) != NULL)
	{
		info += Basic_String(path);
		memset(path, 0, 1035 * sizeof(char));
	}

	auto pos1 = info.find(':');
	if (pos1 != Basic_String::npos)
	{
		Basic_String lineNumStr = info.substr(pos1 + 1);
		debugInfo.line = std::atoi(lineNumStr.c_str());
		debugInfo.file = info.substr(0, pos1);
	}

	/* close */
	pclose(fp);

	*ln = debugInfo.line;
	strcpy(fn, debugInfo.file.c_str());
}

void *UnixPageAllignedAlloc(size_t size)
{
	// calculate page size
	size_t pageSize = MIN_PAGE_SIZE;
	if (size > MIN_PAGE_SIZE)
	{
		pageSize = size + MIN_PAGE_SIZE - (size % MIN_PAGE_SIZE);
	}
	// allocate pages
	void *p = mmap(nullptr, pageSize + MIN_PAGE_SIZE, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
	if (p != MAP_FAILED)
	{
		// commit all but the last page
		mprotect(p, pageSize, PROT_READ | PROT_WRITE);

		// get pointer to start of object
		unsigned char *obj = (unsigned char *)p + (pageSize - size);

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

void UnixPageAllignedFree(void *p, size_t size)
{
	if (p == nullptr)
	{
		return;
	}

	// decommit all pages for this object
	munmap(p, size );
}

void UnixPageRelease(void *p, size_t size)
{
	if (p == nullptr)
	{
		return;
	}

	void *page = (void *)((char*)p + size);

	munmap(page, MIN_PAGE_SIZE);
}

void *operator new(size_t size)
{
	return MemoryDebugger::GetInstance().Allocate(size, MemoryDebugger::CreateHeader(), __builtin_return_address(0));
}
void *operator new(size_t size, const std::nothrow_t &) noexcept
{
	return MemoryDebugger::GetInstance().Allocate(size, MemoryDebugger::CreateHeader(), __builtin_return_address(0), false);
}
void *operator new[](size_t size)
{
	return MemoryDebugger::GetInstance().Allocate(size, MemoryDebugger::CreateHeader(true), __builtin_return_address(0));
}
void *operator new[](size_t size, const std::nothrow_t &) noexcept
{
	return MemoryDebugger::GetInstance().Allocate(size, MemoryDebugger::CreateHeader(true), __builtin_return_address(0), false);
}

void operator delete(void *address) noexcept
{
	MemoryDebugger::GetInstance().Deallocate(address, __builtin_return_address(0), false, false);
}
void operator delete(void *address, size_t size) noexcept
{
	size = 0;
	MemoryDebugger::GetInstance().Deallocate(address, __builtin_return_address(0), false, false);
}
void operator delete(void *address, const std::nothrow_t &) noexcept
{
	MemoryDebugger::GetInstance().Deallocate(address, __builtin_return_address(0), true, false);
}
void operator delete[](void *address) noexcept
{
	MemoryDebugger::GetInstance().Deallocate(address, __builtin_return_address(0), false, true);
}
void operator delete[](void *address, size_t size) noexcept
{
	size = 0;
	MemoryDebugger::GetInstance().Deallocate(address, __builtin_return_address(0), false, true);
}
void operator delete[](void *address, const std::nothrow_t &) noexcept
{
	MemoryDebugger::GetInstance().Deallocate(address, __builtin_return_address(0), true, true);
}

void *operator new(size_t size, const char *tag)
{
	return MemoryDebugger::GetInstance().Allocate(size, MemoryDebugger::CreateHeader(), __builtin_return_address(0), true, tag);
}
void *operator new(size_t size, const std::nothrow_t &, const char *tag) noexcept
{
	return MemoryDebugger::GetInstance().Allocate(size, MemoryDebugger::CreateHeader(), __builtin_return_address(0), false, tag);
}
void *operator new[](size_t size, const char *tag)
{
	return MemoryDebugger::GetInstance().Allocate(size, MemoryDebugger::CreateHeader(true), __builtin_return_address(0), true, tag);
}
void *operator new[](size_t size, const std::nothrow_t &, const char *tag) noexcept
{
	return MemoryDebugger::GetInstance().Allocate(size, MemoryDebugger::CreateHeader(true), __builtin_return_address(0), false, tag);
}

#endif