# An Efficient C++ Fixed Block Memory Allocator

The original article is available [here](http://www.codeproject.com/Articles/1083210/An-efficient-Cplusplus-fixed-block-memory-allocato).

**Summary:** A fixed block memory allocator that increases system performance and offers heap fragmentation fault protection

## Introduction

Custom fixed block memory allocators are used to solve at least two types of memory related problems. First, global heap allocations/deallocations can be slow and nondeterministic. You never know how long the memory manager is going to take. Secondly, to eliminate the possibility of a memory allocation fault caused by a fragmented heap – a valid concern, especially on mission-critical type systems.

Even if the system isn't considered mission-critical, some embedded systems are designed to run for weeks or years without a reboot. Depending on allocation patterns and heap implementation, long-term heap use can lead to heap faults.

The typical solution is to statically declare all objects up front and get rid of dynamic allocation altogether. However, static allocation can waste storage, because the objects, even if they're not actively being used, exist and take up space. In addition, implementing a system around dynamic allocation can offer a more natural design architecture, as opposed to statically declaring all objects.

Fixed block memory allocators are not a new idea. People have been designing various sorts of custom memory allocators for a long time. What I am presenting here is a simple C++ allocator implementation that I've used successfully on a variety of projects.

The solution presented here will:


- Be faster than the global heap
- Eliminate heap fragmentation memory faults
- Require no additional storage overhead (except for a few bytes of static memory)
- Be easy to use
- Use minimal code space

A simple class that dispenses and reclaims memory will provide all of the aforementioned benefits, as I'll show.

After reading this article, be sure to read the follow-on article ["Replace malloc/free with a Fast Fixed Block Memory Allocator"](http://www.codeproject.com/Articles/1084801/Replace-malloc-free-with-a-fast-fixed-block-memory) to see how `Allocator` is used to create a really fast `malloc()` and `free()` CRT replacement.

## Storage Recycling

The basic philosophy of the memory management scheme is to recycle memory obtained during object allocations. Once storage for an object has been created, it's never returned to the heap. Instead, the memory is recycled, allowing another object of the same type to reuse the space. I've implemented a class called `Allocator` that expresses the technique.

When the application deletes using `Allocator`, the memory block for a single object is freed for use again but is not actually released back to the memory manager. Freed blocks are retained in a linked list, called the free-list, to be doled out again for another object of the same type. On every allocation request, `Allocator` first checks the free-list for an existing memory block. Only if none are available is a new one created. Depending on the desired behavior of `Allocator`, storage comes from either the global heap or a `static` memory pool with one of three operating modes:

1. Heap blocks
2. Heap pool
3. Static pool

## Heap vs. Pool

The `Allocator` class is capable of creating new blocks from the heap or a memory pool whenever the free-list cannot provide an existing one. If the pool is used, you must specify the number of objects up front. Using the total objects, a pool large enough to handle the maximum number of instances is created. Obtaining block memory from the heap, on the other hand, has no such quantity limitations – construct as many new objects as storage permits.

The *heap blocks* mode allocates from the global heap a new memory block for a single object as necessary to fulfill memory requests. A deallocation puts the block into a free list for later reuse. Creating fresh new blocks off the heap when the free-list is empty frees you from having to set an object limit. This approach offers dynamic-like operation since the number of blocks can expand at run-time. The disadvantage is a loss of deterministic execution during block creation

The *heap pool* mode creates a single pool from the global heap to hold all blocks. The pool is created using `operator new` when the `Allocator` object is constructed. `Allocator` then provides blocks of memory from the pool during allocations.

The *static pool* mode uses a single memory pool, typically located in static memory, to hold all blocks. The static memory pool is not created by `Allocator` but instead is provided by the user of the class.

The heap pool and static pool modes offers consistent allocation execution times because the memory manager is never involved with obtaining individual blocks. This makes a new operation very fast and deterministic.

## Class Design

The class interface is really straightforward. `Allocate()` returns a pointer to a memory block and `Deallocate()` frees the memory block for use again. The constructor is responsible for setting the object size and, if necessary, allocating a memory pool.

The arguments passed into the class constructor determine where the new blocks will be obtained. The `size` argument controls the fixed memory block size. The `objects` argument sets how many blocks are allowed. `0` means get new blocks from the heap as necessary, whereas any other non-zero value indicates using a pool, heap or static, to handle the specified number of instances. The `memory` argument is a pointer to a optional `static` memory. If `memory` is `0` and `objects` is not `0`, `Allocator` will create a pool from the heap. The static memory pool must be `size` x `objects` bytes in size. The `name` argument optionally gives the allocator a name, which is useful for gather allocator usage metrics.

```
    class Allocator
    {
    public:
       Allocator(size_t size, UINT objects=0, CHAR* memory=NULL, const CHAR* name=NULL);
```

The examples below show how each of the three operational mode is controlled via the constructor arguments.

```
    // Heap blocks mode with unlimited 100 byte blocks
    Allocator allocatorHeapBlocks(100);

    // Heap pool mode with 20, 100 byte blocks
    Allocator allocatorHeapPool(100, 20);

    // Static pool mode with 20, 100 byte blocks
    char staticMemoryPool[100 * 20];
    Allocator allocatorStaticPool(100, 20, staticMemoryPool);
```

To simplify the `static` pool method somewhat, a simple `AllocatorPool<>` template class is used. The first template argument is block type and the second argument is the block quantity.

```
    // Static pool mode with 20 MyClass sized blocks 
    AllocatorPool<MyClass, 20> allocatorStaticPool2;
```

By calling `Allocate()`, a pointer to a memory block the size of one instance is returned. When obtaining the block, the free-list is checked to determine if a fee block already exists. If so, just unlink the existing block from the free-list and return a pointer to it; otherwise create a new one from the pool/heap.

```
    void* memory1 = allocatorHeapBlocks.Allocate(100);
```

`Deallocate()` just pushes the block address onto a stack. The stack is actually implemented as a singly linked list (the free-list), but the class only adds/removes from the head so the behavior is that of a stack. Using a stack makes the allocation/deallocations really fast. No searching through a list – just push or pop a block and go.
```
    allocatorHeapBlocks.Deallocate(memory1);
```

Now comes a handy for linking blocks together in the free-list without consuming any extra storage for the pointers. If, for example, we use the global `operator new`, storage is allocated first then the constructor is called. The destruction process is just the reverse; destructor is called, then memory freed. After the destructor is executed, but before the storage is released back to the heap, the memory is no longer being utilized by the object and is freed to be used for other things, like a next pointer. Since the `Allocator` class needs to keep the deleted blocks around, during `operator delete` we put the list's next pointer in that currently unused object space. When the block is reused by the application, the pointer is no longer needed and will be overwritten by the newly formed object. This way, there is no per-instance storage overhead incurred.

Using freed object space as the memory to link blocks together means the object must be large enough to hold a pointer. The code in the constructor initializer list ensures the minimum block size is never below the size of a pointer.

The class destructor frees the storage allocated during execution by deleting the memory pool or, if blocks were obtained off the heap, by traversing the free-list and deleting each block. Since the `Allocator` class is typically used as a class scope `static`, it will only be called upon termination of the program. For most embedded devices, the application is terminated when someone yanks the power from the system. Obviously, in this case, the destructor is not required.

If you're using the heap blocks method, the allocated blocks cannot be freed when the application terminates unless all the instances are checked into the free-list. Therefore, all outstanding objects must be "deleted" before the program ends. Otherwise, you've got yourself a nice memory leak. Which brings up an interesting point. Doesn't `Allocator` have to track both the free and used blocks? The short answer is no. The long answer is that once a block is given to the application via a pointer, it then becomes the application's responsibility to return that pointer to `Allocator` by means of a call to `Deallocate()` before the program ends. This way, we only need to keep track of the freed blocks.

## Using the Code

I wanted `Allocator` to be extremely easy to use, so I created macros to automate the interface within a client class. The macros provide a `static` instance of `Allocator` and two member functions: `operator new` and `operator delete`. By overloading the `new` and `delete` operators, `Allocator` intercepts and handles all memory allocation duties for the client class.

The `DECLARE_ALLOCATOR` macro provides the header file interface and should be included within the class declaration like this:

```
    #include "Allocator.h"
    class MyClass
    {
        DECLARE_ALLOCATOR
        // remaining class definition
    };
```

The `operator new` function calls `Allocator` to create memory for a single instance of the class. After the memory is allocated, by definition, the `operator new` calls the appropriate constructor for the class. When overloading `new` only the memory allocation duties can be taken over. The constructor call is guaranteed by the language. Similarly, when deleting an object, the system first calls the destructor for us, and then `operator delete` is executed. The `operator delete` uses the `Deallocate()` function to store the memory block in the free-list.

Among C++ programmers, it's relatively common knowledge that when deleting a class using a base pointer, the destructor should be declared virtual. This ensures that when deleting a class, the correct derived destructor is called. What is less apparent, however, is how the virtual destructor changes which class's overloaded `operator delete` is called.

Although not explicitly declared, the `operator delete` is a `static` function. As such, it cannot be declared virtual. So at first glance, one would assume that deleting an object with a base pointer couldn't be routed to the correct class. After all, calling an ordinary `static` function with a base pointer will invoke the base member's version. However, as we know, calling an `operator delete` first calls the destructor. With a virtual destructor, the call is routed to the derived class. After the class's destructor executes, the `operator delete` for that derived class is called. So in essence, the overloaded `operator delete` was routed to the derived class by way of the virtual destructor. Therefore, if deletes using a base pointer are performed, the base class destructor must be declared virtual. Otherwise, the wrong destructor and overloaded `operator delete` will be called.

The `IMPLEMENT_ALLOCATOR` macro is the source file portion of the interface and should be placed in file scope.

```
   IMPLEMENT_ALLOCATOR(MyClass, 0, 0)
```

Once the macros are in place, the caller can create and destroy instances of this class and deleted object stored will be recycled:

```
    MyClass* myClass = new MyClass();
    delete myClass;
```

Both single and multiple inheritance situations work with the `Allocator` class. For example, assuming the class `Derived` inherits from class `Base` the following code fragment is legal.

```
    Base* base = new Derived;
    delete base;
```

## Run Time

At run time, `Allocator` will initially have no blocks within the free-list, so the first call to `Allocate()` will get a block from the pool or heap. As execution continues, the system demand for objects of any given allocator instance will fluctuate and new storage will only be allocated when the free-list cannot offer up an existing block. Eventually, the system will settle into some peak number of instances so that each allocation will be obtained from existing blocks instead of the pool/heap.

Compared to obtaining all blocks using the memory manager, the class saves a lot of processing power. During allocations, a pointer is just popped off the free-list, making it extremely quick. Deallocations just push a block pointer back onto the list, which is equally as fast.

## Benchmarking

Benchmarking the `Allocator` performance vs. the global heap on a Windows PC shows just how fast the class is. An basic test of allocating and deallocating 20000 4096 and 2048 sized blocks in a somewhat interleaved fashion tests the speed improvement. See the attached source code for the exact algorithm.

#### Windows Allocation Times in Milliseconds

--------------------------------------------------
|Allocator  |Mode        |Run|Benchmark Time (mS)|
--------------------------------------------------
|Global Heap|Debug Heap  |1  |1640               |
--------------------------------------------------
|Global Heap|Debug Heap  |2  |1864               |
--------------------------------------------------
|Global Heap|Debug Heap  |3  |1855               |
--------------------------------------------------
|Global Heap|Release Heap|1  |55                 |
--------------------------------------------------
|Global Heap|Release Heap|2  |47                 |
--------------------------------------------------
|Global Heap|Release Heap|3  |47                 |
--------------------------------------------------
|Allocator  |Static Pool |1  |19                 |
--------------------------------------------------
|Allocator  |Static Pool |2  |7                  |
--------------------------------------------------
|Allocator  |Static Pool |3  |7                  |
--------------------------------------------------
|Allocator  |Heap Blocks |1  |30                 |
--------------------------------------------------
|Allocator  |Heap Blocks |2  |7                  |
--------------------------------------------------
|Allocator  |Heap Blocks |3  |7                  |
--------------------------------------------------

Windows uses a debug heap when executing within the debugger. The debug heap adds extra safety checks slowing its performance. The release heap is much faster as the checks are disabled. The debug heap can be disabled within&nbsp;Visual Studio by setting ***_NO_DEBUG_HEAP=1*** in the ***Debugging > Environment*** project option.

The debug global heap is predictably the slowest at about 1.8 seconds. The release heap is much faster at ~50mS. This benchmark test is very simplistic and a more realistic scenario with varying blocks sizes and random new/delete intervals might produce different results. However, the basic point is illustrated nicely; the memory manager is slower than allocator and highly dependent on the platform's implementation.

The `Allocator` running in static pool mode doesn't rely upon the heap. This has a fast execution time of around 7mS once the free-list is populated with blocks. The 19mS on Run 1 accounts for dicing up the fixed memory pool into individual blocks on the first run.

The Allocator running heap blocks mode is just as fast once the free-list is populated with blocks obtained from the heap. Recall that the heap blocks mode relies upon the global heap to get new blocks, but then recycles them into the free-list for later use. Run 1 shows the allocation hit creating the memory blocks at 30mS. Subsequent benchmarks clock in a very fast 7mS since the free-list is fully populated.

As the benchmarking shows, the `Allocator` is highly efficient and about seven times faster than the Windows global release heap.

For comparison on an embedded system, I ran the same tests using Keil running on an ARM STM32F4 CPU at 168MHz. I had to lower the maximum blocks to 500 and the blocks sizes to 32 and 16 bytes due to the constrained resources. Here are the results.

#### ARM STM32F4 Allocation Times in Milliseconds

--------------------------------------------------
|Allocator  |Mode        |Run|Benchmark Time (mS)|
--------------------------------------------------
|Global Heap|Release     |1  |11.6               |
--------------------------------------------------
|Global Heap|Release     |2  |11.6               |
--------------------------------------------------
|Global Heap|Release     |3  |11.6               |
--------------------------------------------------
|Allocator  |Static Pool |1  |0.85               |
--------------------------------------------------
|Allocator  |Static Pool |2  |0.79               |
--------------------------------------------------
|Allocator  |Static Pool |3  |0.79               |
--------------------------------------------------
|Allocator  |Heap Blocks |1  |1.19               |
--------------------------------------------------
|Allocator  |Heap Blocks |2  |0.79               |
--------------------------------------------------
|Allocator  |Heap Blocks |3  |0.79               |
--------------------------------------------------

As the ARM benchmark results show, the `Allocator` class is about 15 times faster which is quite significant. It should be noted that the benchmark test really aggravated the Keil heap. The benchmark test allocates, in the ARM case, 500 16-byte blocks. Then every other 16-byte block is deleted followed by allocating 500 32-byte blocks. That last group of 500 allocations added 9.2mS to the overall 11.6mS time. What this says is that when the heap gets fragmented, you can expect the memory manager to take longer with non-deterministic times.

## Allocator Decisions

The first decision to make is do you need an allocator at all. If you don't have an execution speed or fault-tolerance requirement for your project, you probably don't need a custom allocator and the global heap will work just fine.

On the other hand, if you do require speed and/or fault-tolerance, the allocator can help and the mode of operation depends on your project requirements. An architect for a mission critical design may forbid all use of the global heap. Yet dynamic allocation may lead to a more efficient or elegant design. In this case, you could use the heap blocks mode during debug development to gain memory usage metrics, then for release switch to the `static` pool method to create statically allocated pools thus eliminating all global heap access. A few compile-time macros switch between the modes.

Alternatively, the heap blocks mode may be fine for the application. It does utilize the heap to obtain new blocks, but does prevent heap-fragmentation faults and speeds allocations once the free-list is populated with enough blocks.

While not implemented in the source code due to multi-threaded issues outside the scope of this article, it is easy have the `Allocator` constructor keep a `static` list of all constructed instances. Run the system for a while, then at some pointer iterate through all allocators and output metrics like block count and name via the `GetBlockCount()` and `GetName()` functions. The usage metrics provide information on sizing the fixed memory pool for each allocator when switching over to a memory pool. Always add a few more blocks than maximum measured block count to give the system some added resiliency against the pool running out of memory.

## Debugging Memory Leaks

Debugging memory leaks can be very difficult, mostly because the heap is a black box with no visibility into the types and sizes of objects allocated. With `Allocator`, memory leaks are a bit easier to find since the `Allocator` tracks the total block count. Repeatedly output (to the console for example) the `GetBlockCount()` and `GetName()` for each allocator instance and comparing the differences should expose the allocator with an ever increasing block count.

## Error Handling

Allocation errors in C++ are typically caught using the new-handler function. If the memory manager faults while attempting to allocate memory off the heap, the user's error-handling function is called via the new-handler function pointer. By assigning the user's function address to the new-handler, the memory manager is able to call a custom error-handling routine. To make the `Allocator` class's error handling consistent, allocations that exceed the pool's storage capacity also call the function pointed to by new-handler, centralizing all memory allocation faults in one place.

```
    static void out_of_memory()
    {
        // new-handler function called by Allocator when pool is out of memory
        assert(0);
    }
    
    int _tmain(int argc, _TCHAR* argv[])
    {
        std::set_new_handler(out_of_memory);
        ...
```

## Limitations

The class does not support arrays of objects. An overloaded `operator new[]` poses a problem for the object recycling method. When this function is called, the `size_t` argument contains the total memory required for all of the array elements. Creating separate storage for each element is not an option because multiple calls to new don't guarantee that the blocks are within a contiguous space, which an array needs. Since `Allocator` only handles same size blocks, arrays are not allowed.

## Porting Issues

`Allocator` calls the `new_handler()` directly when the `static` pool runs out of storage, which may not be appropriate for some systems. This implementation assumes the new-handler function will not return, such as an infinite loop trap or assertion, so it makes no sense to call the handler if the function resolves allocation failures by compacting the heap. A fixed pool is being used and no amount of compaction will remedy that.

## Reference articles

- [Replace malloc/free with a Fast Fixed Block Memory Allocator](http://www.codeproject.com/Articles/1084801/Replace-malloc-free-with-a-fast-fixed-block-memory) to see how `Allocator` is used to create a really fast `malloc()` and `free()` CRT replacement.

