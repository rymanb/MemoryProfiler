#include "MemoryDebugger.h"
#include "Common.h"


#define MAX_TAG_LENGTH 16 // max length of tag
#define MAX_BUFFER_LENGTH 1024 // max length of buffers


MemoryDebugger* MemoryDebugger::mInstance = nullptr; // static member initialization
int MemoryDebuggerInitializer::s_count = 0; // static member initialization


static int frame = 0; // frame counter for statistics


// make sure that the memory debugger is initialized before main
MemoryDebuggerInitializer::MemoryDebuggerInitializer()
{
	// if this is the first instance of the memory debugger
	if (++s_count == 1)
	{
		// initialize the memory debugger
		MemoryDebugger::mInstance = static_cast<MemoryDebugger*>(malloc(sizeof(MemoryDebugger)));
		MemoryDebugger::mInstance = new (MemoryDebugger::mInstance) MemoryDebugger; // Placement new
		Initialize();
	}
}

// make sure that the memory debugger is destroyed after main
MemoryDebuggerInitializer::~MemoryDebuggerInitializer()
{
	// if this is the last instance of the memory debugger
	if (--s_count == 0)
	{
		// output the leaks and errors
		MemoryDebugger::mInstance->PrintLeaks();

		// free the tags
		for (auto& it : MemoryDebugger::mInstance->mTagList)
		{
			char* str = it.second;
			free(str);
		}

		// destroy the memory debugger
		MemoryDebugger::mInstance->~MemoryDebugger();
		free(MemoryDebugger::mInstance);
	}
}

// prints all leaks and memory errors to a csv file
void MemoryDebugger::PrintLeaks()
{
	// add leaks to the error list
	for (auto& address : MemoryDebugger::mInstance->allocated_list)
	{
		errors[address] = { MemoryDebugger::Error::MemoryLeak, mInstance->location_map[address].first };
	}

	FILE* file = nullptr; // file pointer

#ifdef _MSC_VER
	fopen_s(&file, "DebugLog.csv", "w"); // open the file
#else
	file = fopen("DebugLog.csv", "w");
#endif
	
	// make sure the file opened
	if (file == nullptr)
	{
		return;
	}
	
	// write header
	fprintf(file, "Message,File,Line,Bytes,Address,Additional Info\n");

	// write all errors to the file
	for (auto& err : errors)
	{

		char fileName[MAX_BUFFER_LENGTH]; // file name buffer
		int lineNumber = 0; // line number

		auto code = err.second.first; // error code
		auto caller = err.second.second; // caller address

		if (caller == nullptr)
		{
			continue;
		}

		// get the file name and line number from the caller address
		GetSymbols(caller, fileName, &lineNumber);

		char buffer[MAX_BUFFER_LENGTH]; // buffer for the error message

		// write the error type to the buffer
		switch (code)
		{
		case MemoryLeak:
			std::snprintf(buffer, MAX_BUFFER_LENGTH, "Memory leak");
			break;
		case DoubleDelete:
			std::snprintf(buffer, MAX_BUFFER_LENGTH, "Double delete");
			break;
		case ScalarNewVectorDelete:
			std::snprintf(buffer, MAX_BUFFER_LENGTH, "Scalar new used with vector delete");
			break;
		case VectorNewScalarDelete:
			std::snprintf(buffer, MAX_BUFFER_LENGTH, "Vector new used with scalar delete");
			break;
		case InvalidPointerDelete:
			std::snprintf(buffer, MAX_BUFFER_LENGTH, "Invalid Pointer Delete");
			break;

		default:
			break;
		}

		// write the error to the file
		#ifdef _MSC_VER
			fprintf(file, "%s,%s,%d,%d,0x%p, Instruction Address: 0x%p\n", buffer, fileName, lineNumber, (int)mInstance->location_map[err.first].second, err.first, caller);
		#else
			fprintf(file, "%s,%s,%d,%d,%p, Instruction Address: %p\n", buffer, fileName, lineNumber, (int)mInstance->location_map[err.first].second, err.first, caller);
		#endif


	}

	// close the file
	fclose(file);
}

// frees the memory at the given address
void MemoryDebugger::Deallocate(MemoryAddress address, void* caller, bool throw_on_fail, bool isArray)
{
	// deallocate never throws so we don't need to check for that
	if (throw_on_fail)
	{ }
	
	// check if the address is valid
	if (!FindAddress(address))
	{
		// invalid address
		DEBUG_BREAKPOINT();
		mInstance->errors[address] = { Error::InvalidPointerDelete, caller };
	}
	else if (address != nullptr)
	{
		// get pointer to header
		void* realAddress = (void*)((unsigned char*)address - sizeof(Header));
		Header* header = (Header*)realAddress;

		// if object is in the free list we have a double delete
		if (mInstance->needsRelease_list.find(address) != mInstance->needsRelease_list.end())
		{
			DEBUG_BREAKPOINT();
			mInstance->errors[address] = { Error::DoubleDelete, caller };

		}
		// if the objects delete type doesent match the object type we have a wrong delete
		else if (header->isArray != isArray)
		{
			DEBUG_BREAKPOINT();

			// if the object is an array and we are trying to delete it with scalar delete
			if (header->isArray)
			{
				mInstance->allocated_list.remove(address);
				mInstance->errors[address] = { Error::VectorNewScalarDelete, caller };

			}
			// if it is not and array we are deleting a scalar with a vector delete
			else
			{
				mInstance->allocated_list.remove(address);
				mInstance->errors[address] = { Error::ScalarNewVectorDelete, caller };
			}
		}
		else
		{
			// get the size of the page
			size_t pageSize = MIN_PAGE_SIZE;
			if (mInstance->location_map[address].second > MIN_PAGE_SIZE)
			{
				pageSize = header->pageSize;
			}
			
			// add the address to the free list
			mInstance->needsRelease_list[address] = header->pageSize;

			// get the page address
			size_t offset = pageSize - (mInstance->location_map[address].second);
			void* page = (void*)((unsigned char*)address - offset);

			// remove the address from the allocated list
			mInstance->allocated_list.remove(address);

			// update stats
			mCurrentStats.totalFrees++;
			mCurrentStats.deltaMemory -= (int)location_map[address].second;

#ifdef DEBUG
			// memset freed memory to be 0xfe
			memset(address, 0xfe, mInstance->location_map[address].second);
#endif // DEBUG


			
			// free the memory
			PageAllignedFree(page, pageSize);

		}
	}
}

// allocates memory for the given size
MemoryAddress MemoryDebugger::Allocate(size_t size, Header options, void* caller, bool throw_on_fail, const char* tag)
{
	
	// allocate the memory
	MemoryAddress address = PageAllignedAlloc(size + sizeof(Header));

	// update stats
	mCurrentStats.totalAllocations++;
	mCurrentStats.deltaMemory += (int)size;

	// if the allocation failed
	if (address == nullptr)
	{
		if (throw_on_fail)
		{
			throw std::bad_alloc();
		}
		else
		{
			return nullptr; // return null if we are not throwing
		}
	}
	else
	{
		// get and set the header
		Header* header = (Header*)address;
		*header = options;

		// find the page size needed
		if (size + sizeof(Header) > MIN_PAGE_SIZE)
		{
			size_t fs = size + sizeof(Header);
			header->pageSize = fs + MIN_PAGE_SIZE - (fs % MIN_PAGE_SIZE);
		}
		else
		{
			header->pageSize = MIN_PAGE_SIZE; // set the page size to the minimum
		}

		address = (unsigned char*)address + sizeof(Header); // move the address to the start of the data

		// if we have a tag
		if (tag != nullptr)
		{
			// find if we have a tag already allocated with the same name
			if (mInstance->mTagList.find(tag) == mInstance->mTagList.end())
			{
				// if we don't create a new tag
				char* tagA = (char*)malloc(MAX_TAG_LENGTH);
				if (tagA)
				{
#ifdef _MSC_VER		
					// copy the tag to the new tag
					strncpy_s(tagA, MAX_TAG_LENGTH, tag, MAX_TAG_LENGTH);
#else
					strcpy(tagA, tag);
#endif
					// add the tag to the list for reuse
					mInstance->mTagList[tagA] = tagA;
				}
			}
			
			// update the tag allocations
			mInstance->tagHits[mInstance->mTagList[tag]]++;

		}

		// add the address to the allocated list and the location map
		mInstance->allocated_list.push_back(address);
		mInstance->location_map[address] = { caller, size };

	}
	
	// return pointer to object
	return address;
}

// releases all pages
void MemoryDebugger::ReleasePages()
{
	// loop through all the pages
	for (auto& page : needsRelease_list)
	{
		// free the page
		size_t offset = page.second - (mInstance->location_map[page.first].second);
		void* pageptr = (void*)((unsigned char*)page.first - offset - sizeof(Header));
		PageRelease(pageptr, page.second);
	}
	
	needsRelease_list.clear();
}



// creates a header for an object
Header MemoryDebugger::CreateHeader(bool isArray)
{
	Header header;
	header.isArray = isArray;
	header.pageSize = 0;
	return header;
}

// finds if an address is in the allocated list or release list
bool MemoryDebugger::FindAddress(MemoryAddress address)
{
	bool found = true;
	if (std::find(mInstance->allocated_list.begin(), mInstance->allocated_list.end(), address) == mInstance->allocated_list.end())
	{
		found = false;
	}
	if (mInstance->needsRelease_list.find(address) != mInstance->needsRelease_list.end())
	{
		found = true;
	}
	if (address == nullptr)
	{
		found = true;
	}

	return found;
}

// updates the stats and tags
void MemoryDebugger::Update()
{
	printf("\033[%d;%dH", 0, 0);
	// update the current frame
	frame++;

	// update max stats
	if (mInstance->mCurrentStats.deltaMemory > mInstance->mMaxStats.deltaMemory)
	{
		mInstance->mMaxStats.deltaMemory = mInstance->mCurrentStats.deltaMemory;
	}
	if (mInstance->mCurrentStats.totalAllocations > mInstance->mMaxStats.totalAllocations)
	{
		mInstance->mMaxStats.totalAllocations = mInstance->mCurrentStats.totalAllocations;
	}
	if (mInstance->mCurrentStats.totalFrees > mInstance->mMaxStats.totalFrees)
	{
		mInstance->mMaxStats.totalFrees = mInstance->mCurrentStats.totalFrees;
	}

	// update frame stats for the current frame
	mInstance->mLastStats = mInstance->mCurrentStats;

	// reset the current stats
	mInstance->mCurrentStats.deltaMemory = 0;
	mInstance->mCurrentStats.totalAllocations = 0;
	mInstance->mCurrentStats.totalFrees = 0;

	// update the tag stats
	tagHitsPrev = tagHits;
	tagHits.clear();
}

// prints the stats
void MemoryDebugger::PrintStats()
{

	printf("+-----------------------------------+\n");
	printf("| Memory Debugger Stats (frame %d)  |\n", frame);
	printf("+-----------------------------------+\n");
	printf("| Total Allocations: %d             |\n", mInstance->mLastStats.totalAllocations);
	printf("| Total Frees: %d                   |\n", mInstance->mLastStats.totalFrees);
	printf("| Delta Memory: %d                  |\n", mInstance->mLastStats.deltaMemory);
	printf("+-----------------------------------+\n");
	printf("| Max Delta Memory: %d              |\n", mInstance->mMaxStats.deltaMemory);
	printf("| Max Total Allocations: %d         |\n", mInstance->mMaxStats.totalAllocations);
	printf("| Max Total Frees: %d               |\n", mInstance->mMaxStats.totalFrees);
	printf("+-----------------------------------+\n");
}

// prints the tag stats
void MemoryDebugger::PrintTagHits()
{
	printf("+------------------------------------+\n");
	printf("| Memory Debugger Tag Hits (frame %d)|\n", frame);
	printf("+------------------------------------+\n");
	for (auto& tag : mInstance->mTagList)
	{
		printf("| %s: %d                           |\n", tag.first, tagHitsPrev[tag.first]);
	}
	printf("+------------------------------------+\n");
}


