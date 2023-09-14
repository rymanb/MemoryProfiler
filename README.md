Memory Debugger
-----------------------
### Compilers  
- Cover which compilers your application targets
    - [ ] gcc [broken on workflow and cmake]  
    - [X] clang [15.0.7]  
    - [x] msvc/Visual Studio [2022]  

### Integration  
- To integrate the Memory debugger into a project, you must add the following files to your project:  
    - MemoryDebugger.h
    - MemoryDebugger.cpp
    - mallocator.h
    - Common.h
    - WindowsController.h
    - WindowsController.cpp
- Next, you need to include the MemoryDebugger.h file in your main.cpp file. This will allow the MemoryDebugger to override the new and delete operators.
- That is all that is required to integrate the MemoryDebugger into your project. And it should automatically start working.

### Requirements  
- The only requirement is that you must include the MemoryDebugger.h file in your main.cpp file. This will allow the operator new and delete to be overridden by the MemoryDebugger class and allow it to track memory allocations and deallocations.
- linux: must have addr2line installed in your /bin/ directory

### Output  
- In the event of a memory leak or other error, that dosent cause a crash, at the end of the program, the memory debugger will create a file in the executable directory called "DebugLog.csv". 
    - This file will contain the following information:
        - Message: The type of error that occured. (Memory Leak, Double Free, wrong delete, invalid pointer)
        - File: The file that the error occured in.
        - Line: The line number that the error occured on.
        - Bytes: The number of bytes that were allocated or deallocated.
        - Address: The address of the memory that was allocated or deallocated.
        - Additional Information: Any additional information that the MemoryDebugger has about the error (such as instruction address)
    - The program will also set off a breakpoint in the debugger, so that you can inspect the error if you are in debug mode, and are not using nothrow operators.
- In the event of a error that causes a crash, such as a buffer overflow, or accessing a freed address, the memory debugger will instantly throw an access violation on the offending line.

### Implementation
- Memory Debugger
    - singleton class that uses the initilizer class example and a static variable to ensure that it is initialized first.
    - Overloads the new and delete operators to call the MemoryDebugger class.
        - These overloads are in a os specific file, so that the MemoryDebugger can be used on both windows and linux.
    - Uses std::list/linkedList to store the allocations
        - This is because it is faster to add to the front of the list, which occurs very often.
    - Uses a map to store to be freed allocations, contains the address of the object and the size the pages - 1 pages
        - this is so that i can later get the start of the page that the object is on and release the page.
    - Uses a map to store the instruction pointer of the size the object was allocated with.
        - This is so that i can get the file and line number of the allocation and deallocation if it is a memory leak.
    - uses a map of memory address to error type + instruction pointer to store additional errors.
        - This is so that i can print out all the errors at the end along with their type and file/line number for example double delete, etc.
    - Also use maps of strings for tagLists so that i can store tags for allocated objects and determine per frame hot spots
    - At the end of the program, it will write out all the errors that occured and the file and line number that they occured on pt DebugLog.csv.
    - Additionally, if the program is in debug mode, I memset no man's land to 0xfd from the start of the first page to the start of the object. i then memset the object with 0xcd. and upon a delete i memset the freed object to 0xfe.
        - This does not actually detect any errors, however it does allow the user to see boundries in the debugger/memory viewer.
        - especially helpful for detecting buffer underflows as they dont cause a crash.
- Memory Allocation
    - Allocates 2 pages of memory at a minimum, and commits the first page but not the second page and returns a pointer to the end of the first page - size of the object.
        - This is so that if the user tries to access after the object, it will cause a page fault and the program will crash. This is how I detect buffer overflows.
    - Stores a header at the beginning of each object that stores information on if an object is an array/scalar, the page size, and tag information.
    - Stores the instruction pointer of the allocation in a map along with the size of allocation, so that I can get the file and line number of the allocation and deallocation if it is a memory leak.
- Memory Deallocation
    - Deallocations simply decommit the total pages - 1 of memory, so that if the user tries to access the memory, it will cause a page fault and the program will crash. This is how I detect accessing freed memory.
    - Deallocations check for double frees using the to be freed list.
    - Invalid pointers if they dont exist in the allocation list or the to be freed list.
    - wrong delete if the objects header says its type does not match the delete operator used.
- Diagram of allocations
    ```
    int* a = new int;
    +------------------------------------------------|-----------------------------------------------+
    |                     Page 1                     |                     Page 2                    |   
    +------------------------------------------------|-----------------------------------------------+
    |        Noman's Land      | Header     | Object |  Uncommitted memory, no read/write            |
    | fdfdfdfdfdfdfdfdfdfdfdfd | 00 00 0..  | cdcdcd | ????????????????????????????????????????????? |
    +------------------------------------------------|-----------------------------------------------+

    int* a = new int[10];
    +------------------------------------------------|-----------------------------------------------+
    |                     Page 1                     |                     Page 2                    |   
    +------------------------------------------------|-----------------------------------------------+
    |        Noman's Land      | Header     | Object |  Uncommitted memory, no read/write            |
    | fdfdfdfdfdfdfdfdfdfdfdfd | 01 00 0..  | cdcdcd | ????????????????????????????????????????????? |
    +------------------------------------------------|-----------------------------------------------+
    ```
- Windows
    - I am using VirtualAlloc and VirtualFree to allocate and deallocate memory.
    - _ReturnAddress() is used to get the instruction pointer of the allocation or deallocation from new/delete;
    - SymGetLineFromAddr64 and its setup frunctions are used to get the file and line number from the instruction pointer saved from the call to new.
        - this is not ran until the end of the program, so it is not a performance hit.

- Linux
    - Use mmap and munmap to alloccate and dealocate memory. and mprotect to protect our last page
    - __builtin_return_address(0) to get the instruction pointer from new and delete
    - To get our line and file location, i use backtrace and backtrace_symbols to obtain the path to our executable. Then pipe that to addr2line along with our caller function address obtained with dladdr1. addr2line then returns us the file and line number.
        - this is not ran until the end of the program, so it is not a performance hit.


- Other Features
    - I implemented memory pattern filling for debug mode only
        - 0xfd is no mans land
        - 0xcd is the unitialized object
        - 0xfe is the freed object
    - I implemented a stat tracking system that tracks the allocations/frees and delta memory per frame, aswell at the maximum of each over the course of the program.
        - To use this feature you must call Update() on the memorydebugger once every frame to update the stats. and printStats() to print the stats to the console.
    - I implemented a tag system that allows you to tag allocations with a string. This is useful for tracking down memory leaks.
        - To use this feature you must call new with the tag parameter ie new( "tag" ) int. 
        - If you would like to track the ammount of allocations per tag per frame you must call Update on the memory debugger once every frame.
            - call PrintTagStats() to print the stats to the console.


