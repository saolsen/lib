#ifndef _lib_h
#define _lib_h

// This library is pretty much a port of the Ion standard library to c99.
// Just about all this code was learned and or copied from @pervognsen, @nothings and @cmuratori.

#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>

// @TODO:
#define INLINE
#define THREADLOCAL

typedef uint8_t  u8;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t  i32;
typedef int64_t  i64;
typedef float    f32;
typedef double   f64;

typedef void* (*AllocFunc)(void* allocator, size_t size, size_t align);
typedef void (*FreeFunc)(void* allocator, void* ptr);

typedef struct {
    AllocFunc alloc;
    FreeFunc free;
} Allocator;

// dynamic arrays only depend on an allocator, not any specific ones.
INLINE void noop_free(void* data, void* ptr) {
}

// @NOTE: This stuff is all direct from ion.
INLINE
void *default_alloc(void *allocator, size_t size, size_t align) {
    // todo: use _aligned_malloc, etc
    return malloc(size);
}

INLINE
void default_free(void *allocator, void *ptr) {
    // todo: use _aligned_free, etc
    free(ptr);
}

THREADLOCAL
Allocator* current_allocator = &(Allocator){default_alloc, default_free};

INLINE
void *generic_alloc(Allocator *allocator, size_t size, size_t align) {
    if (!size) {
        return 0;
    }
    if (!allocator) {
        allocator = current_allocator;
    } 
    return allocator->alloc(allocator, size, align);
}

INLINE
void generic_free(Allocator *allocator, void *ptr) {
    if (!allocator) {
        allocator = current_allocator;
    } 
    allocator->free(allocator, ptr);
}

typedef struct {
    Allocator base;
    void* start;
    void* next;
    void* end;
} TempAllocator;

typedef struct {
    void* ptr;
} TempMark;

void* temp_alloc(void* allocator, size_t size, size_t align) {
    TempAllocator *self = (TempAllocator*)allocator;
    uintptr_t aligned = ((uintptr_t)self->next + align - 1)  & ~(align - 1);
    uintptr_t next = aligned + size;
    if (next > (uintptr_t)(self->end)) {
        return 0;
    }
    self->next = (void*)next;
    return (void*)aligned;
}

TempAllocator temp_allocator(void* buf, size_t size) {
    return (TempAllocator){{temp_alloc, noop_free}, buf, buf, buf+size};
}

TempMark temp_begin(TempAllocator* self) {
    return (TempMark){self->next};
}

void temp_end(TempAllocator *self, TempMark mark) {
    void* ptr = mark.ptr;
    assert(self->start <= ptr && ptr <= self->end);
    self->next = ptr;
}

typedef struct {
    Allocator base;
    Allocator* allocator;
    size_t block_size;
    char** blocks;
    char* next;
    char* end;
} ArenaAllocator;

const size_t ARENA_MIN_BLOCK_SIZE = sizeof(uint64_t);
const size_t ARENA_MIN_BLOCK_ALIGN = sizeof(uint64_t);

// arena allocator
// cyclic (frame) allocator, auto release pool so you can pass pointers and stuff.
// - I think this might just almost be a temp allocator.
// pool allocator

void* arena_alloc_grow(ArenaAllocator* self, size_t size, size_t align) {
    size_t block_size = 2*self->block_size;
    if (block_size < size) {
        block_size = size;
    }
    size_t block_align = ARENA_MIN_BLOCK_ALIGN;
    if (block_align < align) {
        block_align = align;
    }
    char* block = generic_alloc(self->allocator, block_size, block_align);
    if (!block) {
        return 0;
    }
    apush(self->blocks, block);
    self->block_size = block_size;
    self->next = block + size;
    self->end = block + block_size;
    return block;
}

void* arena_alloc(void* allocator, size_t size, size_t align) {
    ArenaAllocator* self = (ArenaAllocator*)allocator;
    size_t aligned = ((uintptr_t)self->next + align - 1) & ~(align - 1);
    size_t next = aligned + size;
    if (next > (uintptr_t)self->end) {
        return arena_alloc_grow(self, size, align);
    }
    self->next = (char*)next;
    return (void*)aligned;
}

void arena_free(ArenaAllocator* self) {
    for (int i = 0; i < alen(self->blocks); i++) {
        generic_free(self->allocator, self->blocks[i]);
    }
    afree(self->blocks);
}

ArenaAllocator arena_allocator(void* allocator) {
    return (ArenaAllocator){{arena_alloc, noop_free}, allocator, ARENA_MIN_BLOCK_SIZE};
}

// a library should never call abort
// like dude you're a library, why would you abort
// abort this execution of the library, not the whole program.

// course grained allocators
// arenas and temp allocators
// maybe the lexer has it's own arena for the name table and the token list
// parser references a name table but has it's own ast arena
// resolver references an ast and owns resolved nodes out of it's own arena.
// they all have temp arenas use for internal stuff.
// easy to manage lifetimes because you just have a few arenas to manage.

// ast_free is freeing the ast arena.

// that's it for memory management, now what about recovery?

// you want to be able to "library abort", don't use exceptions, you use longjmp setjmp.

// there's also some story for resource cleanup that's also coupled to this.
// think of this as an operating system.

// the recovery thing is very cool too.



// func ainit<T>(a: T[], allocator: Allocator*)
// func afree<T>(a: T[])
// func alen<T>(a: T[]): usize
// func acap<T>(a: T[]): usize
// func aclear<T>(a: T)
// func afit<T>(a: T[], mincap: usize)
// func asetlen<T>(a: T[], newlen: usize)
// func asetcap<T>(a: T[], newcap: usize)
// func afill<T>(a: T[], v: T, n: usize)
// func adeli<T>(a: T[], i: usize)
// func adeln<T>(a: T[], i: usize, len: usize)
// func acat<T>(a: T[], src: T const[])
// func acatn<T>(a: T[], src: T const[], srclen: usize)
// func apush<T>(a: T[], v: T)
// func apop<T>(a: T[])
// func adefault<T>(a: T[], v: T)
// func aprintf(a: char[], fmt: char const*, ...)

// Key-indexed functions
// func aget<K, V>(a: {K, V}[], k: K): V
// func agetp<K, V>(a: {K, V}[], k: K): V*
// func ageti<K, V>(a: {K, V}[], k: K): usize
// func aput<K, V>(a: {K, V}[], k: K, v: V)
// func adel<K, V>(a: {K, V}[], k: K)

// Value-indexed functions
// func agetv<T>(a: T[], v: T): bool
// func agetvi<T>(a: T[], v: T): usize
// func aputv<T>(a: T[], v: T)
// func adelv<T>(a: T[], v: T)

// func aindexv<T>(a: T[], new_index: Index)
// func aindex<T>(a: T[], new_index: Index)

// func ahdrsize<T>(a: T[]): usize
// func ahdralign<T>(a: T[]): usize
// func ahdrsize<T>(a: T[]): AHdr*

#endif // _lib_h