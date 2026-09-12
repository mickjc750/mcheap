# MCHEAP

A small linked free-list dynamic memory allocator intended for embedded C applications.

- Single-header, stb-style library
- Fixed-size heap
- Configurable allocation alignment and heap location
- Configurable reallocation policy
- Optional locking mechanism for thread safety
- Heap integrity testing
- Supports sandboxing to allow multiple implementations in a single build
- Requires C99 and GCC extensions
- Test suite using [Greatest](https://github.com/silentbicycle/greatest)

## Usage

In **one** source file, define `MCHEAP_IMPLEMENTATION` before including the header:

```c
#define MCHEAP_IMPLEMENTATION
#include "mcheap.h"
```

Other source files simply include the header:

```c
#include "mcheap.h"
```

## Configuration

The following symbols may be defined before including `mcheap.h`.

### `MCHEAP_SIZE`

Size of the heap in bytes.

Defaults to `1024` and must be a multiple of `MCHEAP_ALIGNMENT`.

### `MCHEAP_ALIGNMENT`

Alignment of all allocations.

Defaults to `__BIGGEST_ALIGNMENT__`.

### `MCHEAP_ADDRESS`

Places the heap at a fixed memory address. This is useful for external RAM or memory not covered by the linker script.

If not defined, the heap is allocated as a static `uint8_t[]` in BSS.

`MCHEAP_ADDRESS` must respect `MCHEAP_ALIGNMENT`.

### Reallocation policy

One of the following policies may be selected. If none is defined, `MCHEAP_REALLOC_POLICY_DEFRAG` is used.

#### `MCHEAP_REALLOC_POLICY_DEFRAG`

Attempts to relocate allocations toward lower addresses. This tends to reduce fragmentation, which is useful on embedded platforms with limited RAM.

#### `MCHEAP_REALLOC_POLICY_EVICT`

Always performs allocate-copy-free.

This simplifies the implementation and makes reallocation behaviour more predictable, as a content copy occurs on every successful reallocation.

#### `MCHEAP_REALLOC_POLICY_RESIZE`

Behaves more like conventional `realloc()`, attempting to resize an allocation in place where possible.

This avoids unnecessary content copies when an allocation can be extended or reduced in place.

### `MCHEAP_SANDBOX`

Makes the public API static, restricting it to the implementation.

This allows multiple independent implementations of MCHEAP in a single build. The test suite uses this feature to test all reallocation policies together.

## API

```c
void*  mcheap_allocate(size_t size);
void*  mcheap_reallocate(void* ptr, size_t size);
void*  mcheap_free(void* ptr);
size_t mcheap_largest_free(void);
bool   mcheap_is_intact(void);
void   mcheap_reinit(void);
```

`mcheap_allocate()` returns an allocation or `NULL` on failure.

`mcheap_reallocate()` resizes an allocation. A `NULL` pointer performs a new allocation. A size of zero frees the allocation and returns `NULL`. On failure the original allocation is preserved.

`mcheap_free()` frees an allocation and always returns `NULL`, allowing:

```c
ptr = mcheap_free(ptr);
```

`mcheap_largest_free()` returns the largest allocation that can currently be made.

`mcheap_is_intact()` tests the integrity of the heap metadata.

`mcheap_reinit()` reinitialises the heap, discarding all existing allocations.

## Thread safety

MCHEAP performs no locking by default.

The following macros may be defined to provide platform-specific locking:

```c
#define mcheap_platform_lock()
#define mcheap_platform_unlock()
```

If not defined, both default to `((void)0)`.

## History

MCHEAP originally included diagnostic features such as tracking allocations against source locations, validating addresses passed to `free`, detecting leaks, invoking an error handler on allocation failure, and printing formatted text directly to heap allocations.

As these features grew, they were moved into the separate [Heaps](https://github.com/mickjc750/heaps) project, which can be used with any allocator. MCHEAP was then reduced back to its primary purpose: memory allocation.