#include <assert.h>

#include "mcode_page.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define ARENA_SIZE BPBXSYN_MCODE_PAGE_SIZE
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

typedef struct arena_header {
    void *page_handle;
    const void *exec;

    void *end;
    block_header_s *next_free;
} arena_header_s;

static bpbxsyn_mcalloc_id bbsyn_default_mcalloc(size_t size, void *userdata,
                                         void **write, const void **exec);
static void bbsyn_default_mcfree(bpbxsyn_mcalloc_id id, void *userdata);

static inline block_header_s *next_block(const arena_header_s *arena,
                                         const block_header_s *block) {
    uintptr_t next = (uintptr_t)block + block->size;
    if (next >= (uintptr_t)arena->end)
        return NULL;
    return (void *)next;
}

bpbxsyn_mcalloc_status_e bpbxsyn_mcode_allocator_new(
    size_t arena_size, bpbxsyn_mcode_allocator_s *alloc)
{
    uint8_t *rw;
    const void *exec;
    void *page_handle;

    size_t arena_size_bytes = arena_size * 1024;

    bpbxsyn_mcalloc_status_e status =
        bbsyn_mcode_page_alloc(arena_size_bytes, &page_handle, (void **)&rw,
                               &exec);
    
    if (status != BPBXSYN_MCALLOC_OK)
        return status;

    arena_header_s *arena_header = (void *)rw;
    uintptr_t arena_end = ALIGN(arena_size_bytes, BLOCK_ALIGNMENT)
                          - BLOCK_ALIGNMENT;
    
    const size_t first_block_ofs =
        ALIGN(sizeof(arena_header_s), BLOCK_ALIGNMENT);
    block_header_s *first_block = (void *)(rw + first_block_ofs);
    *first_block = (block_header_s) {
        .alloc = false,
        .size = arena_end - first_block_ofs
    };
    
    arena_header->next_free = first_block;
    arena_header->end = (void *)((uintptr_t)arena_header + arena_end);
    arena_header->exec = exec;
    arena_header->page_handle = page_handle;

    *alloc = (bpbxsyn_mcode_allocator_s) {
        .alloc = bbsyn_default_mcalloc,
        .free = bbsyn_default_mcfree,
        .userdata = arena_header
    };

    return BPBXSYN_MCALLOC_OK;
}

void bpbxsyn_mcode_allocator_destroy(void *alloc) {
    if (!alloc) return;
    
    arena_header_s *arena = (void *)alloc;
    bbsyn_mcode_page_free(arena->page_handle);
}

bpbxsyn_mcalloc_id bbsyn_default_mcalloc(size_t size, void *userdata,
                                         void **write, const void **exec)
{
    *write = NULL;
    *exec = NULL;
    if (size == 0) return BPBXSYN_MCALLOC_INVALID_ID;

    arena_header_s *arena_hdr = (void *)userdata;

    bbsyn_mcode_page_mlock(arena_hdr->page_handle);

    // minimum size of allocation, accounting for the space needed by the
    // allocation header + alignment to next block
    size_t alloc_size =
        ALIGN(size + sizeof(block_header_s), BLOCK_ALIGNMENT);

    block_header_s *free_block = arena_hdr->next_free;
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

            block_header_s *next = next_block(arena_hdr, free_block);
            
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
                arena_hdr->next_free = new_free_block;
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

        void *ret = (void *)ALIGN(free_block + 1, NATIVE_ALIGN);
        *write = ret;
        *exec = (const void *)( (uintptr_t)arena_hdr->exec
                               + ((uintptr_t)ret - (uintptr_t)arena_hdr) );
        
        bbsyn_mcode_page_munlock(arena_hdr->page_handle);
        return (bpbxsyn_mcalloc_id) free_block;
    }

    bbsyn_mcode_page_munlock(arena_hdr->page_handle);
    return BPBXSYN_MCALLOC_INVALID_ID;
}

void bbsyn_default_mcfree(bpbxsyn_mcalloc_id id, void *userdata) {
    if (!id) return;

    block_header_s *block_hdr = (void *)id;
    arena_header_s *arena_hdr = (void *)userdata;

    bbsyn_mcode_page_mlock(arena_hdr->page_handle);

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
    assert(arena_hdr->next_free->prev_free == NULL);
    if (is_new_block) {
        block_hdr->next_free = arena_hdr->next_free;
        arena_hdr->next_free->prev_free = block_hdr;
        arena_hdr->next_free = block_hdr;
    }

    // then, join contiguous free blocks to the right
    while (true) {
        block_header_s *next = next_block(arena_hdr, block_hdr);
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

    bbsyn_mcode_page_munlock(arena_hdr->page_handle);
}