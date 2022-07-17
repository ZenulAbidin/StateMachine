# Replace malloc/free with a Fast Fixed Block Memory Allocator

The original article is available [here](http://www.codeproject.com/Articles/1084801/Replace-malloc-free-with-a-fast-fixed-block-memory).

**Summary:** Replace malloc/free with xmalloc/xfree is faster than the global heap and prevents heap fragmentation faults.

## Introduction

Custom fixed block allocators are specialized memory managers used to solve performance problems with the global heap. In the article ["An Efficient C++ Fixed Block Memory Allocator"](http://www.codeproject.com/Articles/1083210/An-efficient-Cplusplus-fixed-block-memory-allocato), I implemented an allocator class to improve speed and eliminate the possibility of a fragmented heap memory fault. In this latest article, the `Allocator` class is used as a basis for the `xallocator` implementation to replace `malloc()` and `free()`.

Unlike most fixed block allocators, the `xallocator` implementation is capable of running in a completely dynamic fashion without advanced knowledge of block sizes or block quantity. The allocator takes care of all the fixed block management for you. It is completely portable to any PC-based or embedded system. In addition, it offers insight into your dynamic usage with memory statistics.

In this article, I replace the C library `malloc`/`free` with alternative fixed memory block versions `xmalloc()` and `xfree()`. First, I'll briefly explain the underlying `Allocator` storage recycling method, then present how `xallocator` works.

## Storage Recycling

The basic philosophy of the memory management scheme is to recycle memory obtained during object allocations. Once storage for an object has been created, it's never returned to the heap. Instead, the memory is recycled, allowing another object of the same type to reuse the space. I've implemented a class called `Allocator` that expresses the technique.

When the application deletes using `Allocator`, the memory block for a single object is freed for use again but is not actually released back to the memory manager. Freed blocks are retained in a linked list, called the free-list, to be doled out again for another object of the same type. On every allocation request, `Allocator` first checks the free-list for an existing memory block. Only if none are available is a new one created. Depending on the desired behavior of `Allocator`, storage comes from either the global heap or a static memory pool with one of three operating modes:

1. Heap blocks
2. Heap pool
3. Static pool

## Heap vs. Pool

The `Allocator` class is capable of creating new blocks from the heap or a memory pool whenever the free-list cannot provide an existing one. If the pool is used, you must specify the number of objects up front. Using the total objects, a pool large enough to handle the maximum number of instances is created. Obtaining block memory from the heap, on the other hand, has no such quantity limitations – construct as many new objects as storage permits.

The *heap blocks* mode allocates from the global heap a new memory block for a single object as necessary to fulfill memory requests. A deallocation puts the block into a free-list for later reuse. Creating fresh new blocks off the heap when the free-list is empty frees you from having to set an object limit. This approach offers dynamic-like operation since the number of blocks can expand at run-time. The disadvantage is a loss of deterministic execution during block creation.

The *heap pool* mode creates a single pool from the global heap to hold all blocks. The pool is created using operator new when the `Allocator` object is constructed. `Allocator` then provides blocks of memory from the pool during allocations.

The *static pool* mode uses a single memory pool, typically located in static memory, to hold all blocks. The static memory pool is not created by `Allocator` but instead is provided by the user of the class.

The heap pool and static pool modes offers consistent allocation execution times because the memory manager is never involved with obtaining individual blocks. This makes a new operation very fast and deterministic.

The `Allocator` constructor controls the mode of operation.

```
    class Allocator
    {
    public:
        Allocator(size_t size, UINT objects=0, CHAR* memory=NULL, const CHAR* name=NULL);
    ...
```

Refer to [An Efficient C++ Fixed Block Memory Allocator](http://www.codeproject.com/Articles/1083210/An-efficient-Cplusplus-fixed-block-memory-allocato) for more information on `Allocator`.

## xallocator

The `xallocator` module has six main APIs:

- `xmalloc`
- `xfree`
- `xrealloc`
- `xalloc_stats`
- `xalloc_init`
- `xalloc_destroy`

`xmalloc()` is equivalent to `malloc()` and used in exactly the same way. Given a number of bytes, the function returns a pointer to a block of memory the size requested.

```
    void* memory1 = xmalloc(100);
```

The memory block is at least as large as the user request, but could actually be more due to the fixed block allocator implementation. The additional over allocated memory is called slack but with fine-tuning block size, the waste is minimized, as I'll explain later in the article.

`xfree()` is the CRT equivalent of `free()`. Just pass `xfree()` a pointer to a previously allocated `xmalloc()` block to free the memory for reuse.

```
    xfree(memory1);
```

`xrealloc()` behaves the same as `realloc()` in that it expands or contracts the memory block while preserving the memory block contents.

```
char* memory2 = (char*)xmalloc(24);    
strcpy(memory2, "TEST STRING");
memory2 = (char*)xrealloc(memory2, 124);
xfree(memory2);
```

`xalloc_stats()` outputs allocator usage statistics to the standard output stream. The output provides insight into how many `Allocator` instances are being used, blocks in use, block sizes, and more.

`xalloc_init()` must be called one time before any worker threads start, or in the case of an embedded system, before the OS starts. On a C++ application, this function is called automatically for you. However, it is desirable to call `xalloc_init()` manually is some cases, typically on an embedded system to avoid the small memory overhead involved with the automatic `xalloc_init()`/`xalloc_destroy()` call mechanism.

`xalloc_destroy()` is called when the application exits to clean up any dynamically allocated resources. On a C++ application, this function is called automatically when the application terminates. You must never call `xalloc_destroy()` manually except in programs that use `xallocator` only within C files.

Now, when to call `xalloc_init()` and `xalloc_destroy()` within a C++ application is not so easy. The problem arises with `static` objects. If `xalloc_destroy()` is called too early, `xallocator` may still be needed when a `static` object destructor get called at program exit. Take for instance this class:

```
    class MyClassStatic
    {
    public:
        MyClassStatic() 
        { 
            memory = xmalloc(100); 
        }
        ~MyClassStatic() 
        { 
            xfree(memory); 
        }
    private:
        void* memory;
    };
```

Now create a `static` instance of this class at file scope.

```
static MyClassStatic myClassStatic;
```

Since the object is `static`, the `MyClassStatic` constructor will be called before `main()`, which is okay as I’ll explain in the “Porting issues” section below. However, the destructor is called after `main()` exits which is not okay if not handled correctly. The problem becomes how to determine when to destroy the `xallocator` dynamically allocated resources. If `xalloc_destroy()` is called before `main()` exits, `xallocator` will already be destroyed when `~MyClassStatic()` tries to call `xfree()` causing a bug.

The key to the solution comes from a guarantee in the C++ Standard:

> Quote:
>
> “Objects with static storage duration defined in namespace scope in the same translation unit and dynamically initialized shall be initialized in the order in which their definition appears in the translation unit.”

In other words, `static` object constructors are called in the same order as defined within the file (translation unit). The destruction will reverse that order. Therefore, *xallocator.h* defines a `XallocInitDestroy` class and creates a `static` instance of it.

```
    class XallocInitDestroy
    {
    public:
        XallocInitDestroy();
        ~XallocInitDestroy();
    private:
        static INT refCount;
    };
    static XallocInitDestroy xallocInitDestroy;
```

The constructor keeps track of the total number of `static` instances created and calls `xalloc_init()` on the first construction.

```
    INT XallocInitDestroy::refCount = 0;
    XallocInitDestroy::XallocInitDestroy() 
    { 
        // Track how many static instances of XallocInitDestroy are created
        if (refCount++ == 0)
            xalloc_init();
    }
```

The destructor calls `xalloc_destroy()` automatically when the last instance is destroyed.

```
    XallocDestroy::~XallocDestroy()
    {
        // Last static instance to have destructor called?
        if (--refCount == 0)
            xalloc_destroy();
    }
```

When including *xallocator.h* in a translation unit, `xallocInitDestroy` will be declared first since the `#include` comes before user code. Meaning any other `static` user classes relying on `xallocator` will be declared after `#include "xallocator.h"`. This guarantees that `~XallocInitDestroy()` is called after all user `static` classes destructors are executed. Using this technique, `xalloc_destroy()` is safely called when the program exits without danger of having `xallocator` destroyed prematurely.

`XallocInitDestroy` is an empty class and therefore is 1-byte in size. The cost of this feature is then 1-byte for every translation unit that includes *xallocator.h* with the following exceptions.

1. On an embedded system where the application never exits, the technique is not required *except* if `STATIC_POOLS` mode is used. All references to `XallocInitDestroy` can be safety removed and `xalloc_destroy()` need never be called. However, you must now call `xalloc_init()` manually in `main()` before the `xallocator` API is used.
2. When `xallocator` is included within a C translation unit, a `static` instance of `XallocInitDestroy` is not created. In this case, you must call `xalloc_init()` in main() and `xalloc_destroy()` before main() exits.

To enable or disable automatic `xallocator`initialization and destruction, use the `#define` below:

```
    #define AUTOMATIC_XALLOCATOR_INIT_DESTROY
```

On a PC or similarly equipped high RAM system, this 1-byte is insignificant and in return ensures safe `xallocator` operation in `static` class instances during program exit. It also frees you from having to call `xalloc_init()` and `xalloc_destroy()` as this is handled automatically.

## Overload new and delete

To make the `xallocator` really easy to use, I've created a macro to overload the `new`/`delete` within a class and route the memory request to `xmalloc()`/`xfree()`. Just add the macro `XALLOCATOR` anywhere in your class definition.

```
    class MyClass 
    {
        XALLOCATOR
        // remaining class definition
    };
```

Using the macro, a `new`/`delete` of your class routes the request to `xallocator` by way of the overloaded `new`/`delete`.

```
    // Allocate MyClass using fixed block allocator
    MyClass* myClass = new MyClass();
    delete myClass;
```

A neat trick is to place `XALLOCATOR` within the base class of an inheritance hierarchy so that all derived classes allocate/deallocate using `xallocator`. For instance, say you had a GUI library with a base class.

```
    class GuiBase 
    {
        XALLOCATOR
        // remaining class definition
    };
```

Any `GuiBase` derived class (buttons, widgets, etc...) now uses `xallocator` when `new`/`delete` is called without having to add `XALLOCATOR` to every derived class. This is a powerful means to enable fixed block allocations for an entire hierarchy with a single macro statement.

## Code Implementation

`xallocator` relies upon multiple `Allocator` instances to manage the fixed blocks; each `Allocator` instance handles one block size. Like `Allocator`, `xallocator` is designed to operate in heap blocks or static pool modes. The mode is controlled by the `STATIC_POOLS` define within `xallocator.cpp`.

```
    #define STATIC_POOLS    // Static pools mode enabled</span>
```

In heap blocks mode, `xallocator` creates both `Allocator` instances and new blocks dynamically at runtime based upon the requested block sizes. By default, `xallocator` uses powers of two block sizes. 8, 16, 32, 64, 128, etc... This way, `xallocator` doesn't need to know the block sizes in advance and offers the utmost flexibility.

The maximum number of `Allocator` instances dynamically created by `xallocator` is controlled by `MAX_ALLOCATORS`. Increase or decrease this number as necessary for your target application.

```
    #define MAX_ALLOCATORS  15
```

In static pools mode, `xallocator` relies upon `Allocator` instances created during dynamic initialization (before entering `main()`) and static memory pools to satisfy memory requests. This eliminates all heap access with the tradeoff being the block sizes and pools are of fixed size and cannot expand at runtime.

Using `Allocator` initialization in `STATIC_POOLS` mode is tricky. The problem again lies with user class static constructors which might call into the `xallocator` API during construction/destruction. The C++ standard does not guarantee the order of static constructor calls between translation units during dynamic initialization. Yet, `xallocator` must be initialized before any APIs are executed. Therefore, the first part of the solution is to preallocate enough static memory for each `Allocator` instance. Of course, each allocator can use a different `MAX_BLOCKS` value as required. Using this mode, the global heap is never called.

```
    // Update this section as necessary if you want to use static memory pools.
    // See also xalloc_init() and xalloc_destroy() for additional updates required.
    #define MAX_ALLOCATORS    12
    #define MAX_BLOCKS        32
    
    // Create static storage for each static allocator instance
    CHAR* _allocator8 [sizeof(AllocatorPool<CHAR[8], MAX_BLOCKS>);
    CHAR* _allocator16 [sizeof(AllocatorPool<CHAR[16], MAX_BLOCKS>)];
    CHAR* _allocator32 [sizeof(AllocatorPool<CHAR[32], MAX_BLOCKS>)];
    CHAR* _allocator64 [sizeof(AllocatorPool<CHAR[64], MAX_BLOCKS>)];
    CHAR* _allocator128 [sizeof(AllocatorPool<CHAR[128], MAX_BLOCKS>)];
    CHAR* _allocator256 [sizeof(AllocatorPool<CHAR[256], MAX_BLOCKS>)];
    CHAR* _allocator396 [sizeof(AllocatorPool<CHAR[396], MAX_BLOCKS>)];
    CHAR* _allocator512 [sizeof(AllocatorPool<CHAR[512], MAX_BLOCKS>)];
    CHAR* _allocator768 [sizeof(AllocatorPool<CHAR[768], MAX_BLOCKS>)];
    CHAR* _allocator1024 [sizeof(AllocatorPool<CHAR[1024], MAX_BLOCKS>)];
    CHAR* _allocator2048 [sizeof(AllocatorPool<CHAR[2048], MAX_BLOCKS>)];    
    CHAR* _allocator4096 [sizeof(AllocatorPool<CHAR[4096], MAX_BLOCKS>)];
    
    
    // Array of pointers to all allocator instances
    static Allocator* _allocators[MAX_ALLOCATORS];
```

Then when `xalloc_init()` is called during dynamic initalization (via `XallocInitDestroy()`), placement `new` is used to initialize each `Allocator` instance into the static memory previously reserved.

```
    extern "C" void xalloc_init()
    {
        lock_init();
        
    #ifdef STATIC_POOLS
        // For STATIC_POOLS mode, the allocators must be initialized before any other
        // static user class constructor is run. Therefore, use placement new to initialize
        // each allocator into the previously reserved static memory locations.
        new (&_allocator8) AllocatorPool<CHAR[8], MAX_BLOCKS>();
        new (&_allocator16) AllocatorPool<CHAR[16], MAX_BLOCKS>();
        new (&_allocator32) AllocatorPool<CHAR[32], MAX_BLOCKS>();
        new (&_allocator64) AllocatorPool<CHAR[64], MAX_BLOCKS>();
        new (&_allocator128) AllocatorPool<CHAR[128], MAX_BLOCKS>();
        new (&_allocator256) AllocatorPool<CHAR[256], MAX_BLOCKS>();
        new (&_allocator396) AllocatorPool<CHAR[396], MAX_BLOCKS>();
        new (&_allocator512) AllocatorPool<CHAR[512], MAX_BLOCKS>();
        new (&_allocator768) AllocatorPool<CHAR[768], MAX_BLOCKS>();
        new (&_allocator1024) AllocatorPool<CHAR[1024], MAX_BLOCKS>();
        new (&_allocator2048) AllocatorPool<CHAR[2048], MAX_BLOCKS>();
        new (&_allocator4096) AllocatorPool<CHAR[4096], MAX_BLOCKS>();

        // Populate allocator array with all instances 
        _allocators[0] = (Allocator*)&_allocator8;
        _allocators[1] = (Allocator*)&_allocator16;
        _allocators[2] = (Allocator*)&_allocator32;
        _allocators[3] = (Allocator*)&_allocator64;
        _allocators[4] = (Allocator*)&_allocator128;
        _allocators[5] = (Allocator*)&_allocator256;
        _allocators[6] = (Allocator*)&_allocator396;
        _allocators[7] = (Allocator*)&_allocator512;
        _allocators[8] = (Allocator*)&_allocator768;
        _allocators[9] = (Allocator*)&_allocator1024;
        _allocators[10] = (Allocator*)&_allocator2048;
        _allocators[11] = (Allocator*)&_allocator4096;
    #endif
    }
```

At application exit, the destructor for each `Allocator` is called manually.

```
    extern "C" void xalloc_destroy()
    {
        lock_get();

    #ifdef STATIC_POOLS
        for (INT i=0; i<MAX_ALLOCATORS; i++)
        {
            _allocators[i]->~Allocator();
            _allocators[i] = 0;
        }
    #else
        for (INT i=0; i<MAX_ALLOCATORS; i++)
        {
            if (_allocators[i] == 0)
                break;
            delete _allocators[i];
            _allocators[i] = 0;
        }
    #endif
    lock_release();
    lock_destroy();
}
```

## Hiding the Allocator Pointer

When deleting memory, `xallocator` needs the original `Allocator` instance so the deallocation request can be routed to the correct `Allocator` instance for processing. Unlike `xmalloc()`, `xfree()` does not take a size and only uses a `void*` argument. Therefore, `xmalloc()` actually hides a pointer to the allocator within an unused portion of the memory block by adding an additional 4-bytes (typical `sizeof(Allocator*)`) to the request. The caller gets a pointer to the block’s client region where the hidden allocator pointer is not overwritten.

```
   extern "C" void *xmalloc(size_t size)
   {
       lock_get();
       
       // Allocate a raw memory block
       Allocator* allocator = xallocator_get_allocator(size);
       void* blockMemoryPtr = allocator->Allocate(size);
       
       lock_release();
       
       // Set the block Allocator* within the raw memory block region
       void* clientsMemoryPtr = set_block_allocator(blockMemoryPtr, allocator);
       return clientsMemoryPtr;
    }
```

    When `xfree()` is called, the allocator pointer is extracted from the memory block so the correct `Allocator` instance can be called to deallocate the block.

```
    extern "C" void xfree(void* ptr)
    {
        if (ptr == 0)
            return;
        
        // Extract the original allocator instance from the caller's block pointer
        Allocator* allocator = get_block_allocator(ptr);
        
        // Convert the client pointer into the original raw block pointer
        void* blockPtr = get_block_ptr(ptr);
        
        lock_get();
        
        // Deallocate the block
        allocator->Deallocate(blockPtr);

        lock_release();
    }
```

## Porting Issues

The `xallocator` is thread-safe when the locks are implemented for your target platform. The code provided has Windows locks. For other platforms, you'll need to provide lock implementations for the four functions within *xallocator.cpp*:

- `lock_init()`
- `lock_get()`
- `lock_release()`
- `lock_destroy()`

When selecting a lock, use the fastest OS lock available to ensure xallocator operates as efficiently as possible within a multi-threaded environment. If your system is single threaded, then leave the implementation for each of the above functions empty.

Depending on how `xallocator` is used, it may be called before `main()`. This means `lock_get()`/`lock_release()` can be called before `lock_init()`. Since the system is single threaded at this point, the locks aren’t necessary until the OS kicks off. However, just make sure `lock_get()`/`lock_release()` behaves correctly if `lock_init()` isn’t called first. For instance, the check for `_xallocInitialized` below ensures the correct behavior by skipping the lock until `lock_init()` is called.

```
static void lock_get()
   {
       if (_xallocInitialized == FALSE)
           return;
   
       EnterCriticalSection(&_criticalSection); 
   }
```

## Reducing Slack

`xallocator` may return block sizes larger than the requested amount and the additional unused memory is called slack. For instance, for a request of 33 bytes, `xallocator` returns a block of 64 bytes. The additional memory (64 – (33 + 4) = 27 bytes) is slack and goes unused. Remember, if 33 bytes is requested an additional 4-bytes are required to hold the block size. So if a client requests 64-bytes, really the 128-byte allocator is used because 68-bytes are needed.

Adding additional allocators to handle block sizes other than powers of two offers more block sizes to minimize waste. Run your application and profile your `xmalloc()` requested sizes with a bit of temporary debug code. Then add allocator block sizes for specifically handling those cases where a large number of blocks are being used.

In the code below, an `Allocator` instance is created with a block size of 396 when a block between 257 and 396 is requested. Similarly, a block request of between 513 and 768 results in an `Allocator` to handle 768-byte blocks.

```
    // Based on the size, find the next higher powers of two value.
    // Add sizeof(size_t) to the requested block size to hold the size
    // within the block memory region. Most blocks are powers of two,
    // however some common allocator block sizes can be explicitly defined
    // to minimize wasted storage. This offers application specific tuning.
    size_t blockSize = size + sizeof(Allocator*);
    if (blockSize > 256 && blockSize <= 396)
        blockSize = 396;
    else
         if (blockSize > 512 && blockSize <= 768)
             blockSize = 768;
         else
             blockSize = nexthigher<size_t>(blockSize)
```

With a minor amount of fine-tuning, you can reduce wasted storage due to slack based on your application's memory usage patterns. If no tuning is required and using blocks solely based on powers of two is acceptable, the only lines of code required from the snippet above are:

```
    blockSize = nexthigher<size_t>(size + sizeof(Allocator*));
```

Using `xalloc_stats()`, it’s easy to find which allocators are being used the most.

```
    xallocator Block Size: 128 Block Count: 10001 Blocks In Use: 1
    xallocator Block Size: 16 Block Count: 2 Blocks In Use: 2
    xallocator Block Size: 8 Block Count: 1 Blocks In Use: 0
    xallocator Block Size: 32 Block Count: 1 Blocks In Use: 0
```

## Allocator vs. xallocator

The advantage of using `Allocator` is that the allocator block size exactly the size of the object and the minimum block size is only 4-bytes. The disadvantage is that the `Allocator` instance is `private` and only usable by that class. This means that the fixed block memory pool can't easily be shared with other instances of similarly sized blocks. This can waste storage due to the lack of sharing between memory pools.

`xallocator`, on the other hand, uses a range of different block sizes to satisfy requests and is thread-safe. The advantage is that the various sized memory pools are shared via the `xmalloc`/`xfree` interface, which can save storage, especially if you tune the block sizes for your specific application. The disadvantage is that even with block size tuning, there will always be some wasted storage due to slack. For small objects, the minimum block size is 8-bytes, 4-bytes for the free-list pointer and 4-bytes to hold the block size. This can become a problem with a large number of small objects.

An application can mix `Allocator` and `xallocator` usage in the same program to maximize efficient memory utilization as the designer sees fit.

## Benchmarking

Benchmarking the `xallocator` performance vs. the global heap on a Windows PC shows just how fast it is. An basic test of allocating and deallocating 20000 4096 and 2048 sized blocks in a somewhat interleaved fashion tests the speed improvement. All tests run with maximum compiler speed optimizations. See the attached source code for the exact algorithm.

#### Allocation Times in Milliseconds

--------------------------------------------------
|Allocator  |Mode        |Run|Benchmark Time (mS)|
--------------------------------------------------
|Global Heap|Debug Heap  |1  |1247               |
--------------------------------------------------
|Global Heap|Debug Heap  |2  |1640               |
--------------------------------------------------
|Global Heap|Debug Heap  |3  |1650               |
--------------------------------------------------
|Global Heap|Release Heap|1  |32.9               |
--------------------------------------------------
|Global Heap|Release Heap|2  |33.0               |
--------------------------------------------------
|Global Heap|Release Heap|3  |27.8               |
--------------------------------------------------
|xallocator |Heap Blocks |1  |17.5               |
--------------------------------------------------
|xallocator |Heap Blocks |2  |5.0                |
--------------------------------------------------
|xallocator |Heap Blocks |3  |5.9                |
--------------------------------------------------

Windows uses a debug heap when executing within the debugger. The debug heap adds extra safety checks slowing its performance. The release heap is much faster as the checks are disabled. The debug heap can be disabled within Visual Studio by setting ***_NO_DEBUG_HEAP=1*** in the ***Debugging > Environment*** project option.

The debug global heap is predictably the slowest at about 1.6&nbsp;seconds. The release heap is much faster at ~30mS. This benchmark&nbsp;test is very simplistic and a more realistic scenario with varying blocks sizes and random new/delete intervals might produce different results. However, the basic point is illustrated nicely; the memory manager is slower than allocator and highly dependent on the platform's implementation.

The `xallocator` running heap blocks mode very fast once the free-list is populated with blocks obtained from the heap. Recall that the heap blocks mode relies upon the global heap to get new blocks, but then recycles them into the free-list for later use. Run 1 shows the allocation hit creating the memory blocks at 17mS. Subsequent benchmarks clock in a very fast 5mS since the free-list is fully populated.

As the benchmarking shows, the `xallocator` is highly efficient and about five times faster than the global heap on a Windows PC. On an ARM STM32F4 CPU built using a Keil compiler I've see well over a 10x speed increase.

## Reference articles


- [An Efficient C++ Fixed Block Memory Allocator](http://www.codeproject.com/Articles/1083210/An-Efficient-Cplusplus-Fixed-Block-Memory-Allocato) by David Lafreniere
- [A Custom STL std::allocator Replacement Improves Performance](http://www.codeproject.com/Articles/1089905/A-Custom-STL-std-allocator-Replacement-Improves-Pe) by David Lafreniere

## Conclusion

A medical device I worked on had a commercial GUI library that utilized the heap extensively. The size and frequency of the memory requests couldn’t be predicted or controlled. Using the heap is such an uncontrolled fashion is a no-no on a medical device, so a solution was needed. Luckily, the GUI library had a means to replace `malloc()` and `free()` with our own custom implementation. `xallocator` solved the heap speed and fragmentation problem making the GUI framework a viable solution on that product.

If you have an application that really hammers the heap and is causing slow performance, or if you’re worried about a fragmented heap fault, integrating `Allocator`/`xallocator` may help solve those problems.

