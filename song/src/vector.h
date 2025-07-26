#ifndef _bpbx_vector_h_
#define _bpbx_vector_h_

#include <stdint.h>
#include <stddef.h>

void vector_ptr_init(void **arr, size_t *size, size_t *capacity);
void vector_ptr_free(void **arr, size_t *size, size_t *capacity);
void vector_ptr_reserve(void **arr, size_t *size, size_t *capacity, size_t desired_size);

void vector_ptr_insert(void **arr, size_t *size, size_t *capacity, size_t index, void *item);
void vector_ptr_remove(void **arr, size_t *size, size_t *capacity, size_t index);

#define vector_ptr_push(arr, size, capacity, item) vector_ptr_insert((arr), (size), (capacity), *(size), item)
#define vector_ptr_pop(arr, size, capacity, item) vector_ptr_remove((arr), (size), (capacity), *(size) - 1, item)

#endif