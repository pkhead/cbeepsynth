// TODO: more extensive testing
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>

#include "emalloc.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define NATIVE_ALIGN (sizeof(uintptr_t))
#define BLOCK_ALIGNMENT (ALIGN(sizeof(block_header_s), NATIVE_ALIGN))
#define ALIGN(ptr, alignment) \
    (((uintptr_t)(ptr) + (alignment) - 1) / (alignment) * (alignment))

typedef struct block_header block_header_s;
struct block_header {
    size_t size; // including the header!
    block_header_s *prev;
    block_header_s *next_free;
    block_header_s *prev_free;
    bool alloc;
};

// header for a heap controlled by emalloc
struct emalloc_heap {
    void *end;
    block_header_s *next_free;
};

static inline block_header_s *next_block(const emalloc_heap_s *heap,
                                         const block_header_s *block) {
    uintptr_t next = (uintptr_t)block + block->size;
    if (next >= (uintptr_t)heap->end)
        return NULL;
    return (void *)next;
}

bool bbsyn_emalloc_init(emalloc_heap_s *heap, size_t heap_size) {
    if (heap_size < ALIGN(sizeof(emalloc_heap_s), BLOCK_ALIGNMENT) 
                    + sizeof(block_header_s))
    {
        return false;
    }

    uintptr_t arena_end = ALIGN(heap_size, BLOCK_ALIGNMENT) - BLOCK_ALIGNMENT;
    
    const size_t first_block_ofs =
        ALIGN(sizeof(emalloc_heap_s), BLOCK_ALIGNMENT);
    block_header_s *first_block = (void *)(heap + first_block_ofs);
    *first_block = (block_header_s) {
        .alloc = false,
        .size = arena_end - first_block_ofs
    };
    
    heap->next_free = first_block;
    heap->end = (void *)((uintptr_t)heap + arena_end);
    return true;
}

void *bbsyn_emalloc_malloc(emalloc_heap_s *heap, size_t size) {
    if (size == 0) return NULL;

    // minimum size of allocation, accounting for the space needed by the
    // allocation header + alignment to next block
    size_t alloc_size =
        ALIGN(size + sizeof(block_header_s), BLOCK_ALIGNMENT);

    block_header_s *free_block = heap->next_free;
    block_header_s *prev_free_block = NULL;
    
    // traverse through the linked list of free blocks
    while (free_block) {
        assert(free_block->prev_free == prev_free_block);

        // free block is too small, so continue onward
        if (free_block->size < alloc_size) {
            prev_free_block = free_block;
            free_block = free_block->next_free;
            continue;
        }

        // free block found!
        // first, create a new free block after the allocation
        block_header_s *next_free = free_block->next_free;

        if (free_block->size > alloc_size) {
            block_header_s *new_free_block =
                (void *)((uintptr_t)free_block + alloc_size);

            block_header_s *next = next_block(heap, free_block);
            
            *new_free_block = (block_header_s) {
                .alloc = false,
                .size = free_block->size - alloc_size,
                .prev = free_block,
                .next_free = next_free,
                .prev_free = prev_free_block
            };

            if (next_free)
                next_free->prev_free = new_free_block;

            if (next)
                next->prev = new_free_block;

            if (prev_free_block)
                prev_free_block->next_free = new_free_block;
            else
                heap->next_free = new_free_block;
        } else {
            if (next_free)
                next_free->prev_free = prev_free_block;

            if (prev_free_block)
                prev_free_block->next_free = next_free;
        }

        // convert free block into allocated block
        free_block->alloc = true;
        free_block->size = alloc_size;
        free_block->next_free = NULL;
        free_block->prev_free = NULL;

        return (void *)ALIGN(free_block + 1, NATIVE_ALIGN);
    }

    return NULL;
}

void bbsyn_emalloc_free(emalloc_heap_s *heap, void *ptr) {
    if (!ptr) return;

    block_header_s *block_hdr =
        (block_header_s *)((uintptr_t)ptr - ALIGN(BLOCK_ALIGNMENT, NATIVE_ALIGN));

    assert(block_hdr->alloc);
    assert(block_hdr->next_free == NULL);
    assert(block_hdr->prev_free == NULL);

    // convert this space into a free block
    block_hdr->alloc = false;

    // join contiguous free blocks
    // first, go to the leftmost side of the contiguous block, if it turns out
    // that this block is in the middle of one
    bool is_new_block = true;
    while (true) {
        block_header_s *prev = block_hdr->prev;
        if (!prev || prev->alloc) break;
        block_hdr = prev;
        is_new_block = false;
    }

    // add to next_free linked chain
    assert(heap->next_free->prev_free == NULL);
    if (is_new_block) {
        block_hdr->next_free = heap->next_free;
        heap->next_free->prev_free = block_hdr;
        heap->next_free = block_hdr;
    }

    // then, join contiguous free blocks to the right
    while (true) {
        block_header_s *next = next_block(heap, block_hdr);
        if (!next || next->alloc) break;

        // you cut out the prey block from the linked list (i.e. connect
        // its last to its next).
        if (next->prev_free)
            next->prev_free->next_free = next->next_free;

        // } else {
        //     assert(arena_hdr->next_free == next);
        //     arena_hdr->next_free = next->next_free;
        // }

        if (next->next_free) {
            next->next_free->prev_free = next->prev_free;
        }

        block_hdr->size += next->size;
    }
}