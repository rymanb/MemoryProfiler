#pragma once
#include "Common.h"
#include <list>
#include <map>
#include <unordered_map>
#include <cstdio>
#include <string>
#include <cstring>
#include <algorithm>
#include "OSIntermidiary.h"

/* Definitions *************************************************************************************/
#define MIN_PAGE_SIZE 4096

using MemoryAddress = void*;

struct Statistics
{
	int totalAllocations = 0;
	int totalFrees = 0;
	int deltaMemory = 0;
};

struct MemoryDebuggerInitializer
{
	MemoryDebuggerInitializer();
	~MemoryDebuggerInitializer();
	static int s_count;
};

struct Header
{
	bool isArray = false;
	size_t pageSize = 0;
	char Tag[256] = { 0 };
};


static MemoryDebuggerInitializer s_logInit;

// MemoryDebugger is a singleton class that tracks all memory allocations and frees
class MemoryDebugger
{
	friend MemoryDebuggerInitializer;
	
	// map that uses mallocator
	template<typename U, typename T>
	using map = std::unordered_map<U, T, std::hash<U>, std::equal_to<U>, Mallocator<std::pair<const U, T>>>;

	// list that uses mallocator
	template<typename T>
	using list = std::list<T, Mallocator<T>>;

	// compares char* for map
	struct CompareStrings {
		bool operator()(const char* c1, const  char* c2) const
		{
			return std::strcmp(c1, c2) < 0;
		}
	};

	// map of char* for tags
	template <typename T>
	using strMap = std::map<const char*, T, CompareStrings, Mallocator<std::pair<const char* const, T>>>;

	// error types
	typedef enum Error
	{
		MemoryLeak,
		DoubleDelete,
		ScalarNewVectorDelete,
		VectorNewScalarDelete,
		InvalidPointerDelete,
	} Error;

	Statistics mCurrentStats; // current frame statistics
	Statistics mLastStats;	// last frame statistics
	Statistics mMaxStats; // max statistics of each category
	
	list<MemoryAddress> allocated_list; // holds all allocated memory addresses
	map<MemoryAddress, size_t> needsRelease_list; // holds all memory addresses that need to be released
	map<MemoryAddress, std::pair<void*, size_t>> location_map; // holds all memory addresses and their caller and size
	map<MemoryAddress, std::pair<Error, void*>> errors; // holds all memory addresses and their error type and caller
	strMap<char*> mTagList; // holds all tags and their memory addresses
	strMap<int> tagHits; // holds all tags and how many allocations they have in the current frame
	strMap<int> tagHitsPrev; // holds all tags and how many allocations they have in the last frame

	// prints errors to csv
	void PrintLeaks();

	// finds if a memory address is in the allocated list or free list
	bool FindAddress(MemoryAddress address);

	// releases memory
	void ReleasePages();
	
protected:
	static MemoryDebugger* mInstance; // singleton instance
	 
public:
	// returns the singleton instance
	static MemoryDebugger& GetInstance()
	{		
		return *mInstance;
	}

	// allocates memory and returns the address
	MemoryAddress Allocate(size_t size, Header options, void* caller = nullptr, bool throw_on_fail = true, const char* tag = nullptr);

	// frees memory
	void Deallocate(MemoryAddress address, void* caller = nullptr, bool throw_on_fail = true, bool isArray = false);

	// creates new header
	static Header CreateHeader(bool isArray = false);
	
	// updates the stats and tags
	void Update();
	
	// prints the stats
	void PrintStats();

	// prints the tags
	void PrintTagHits();
};

