#include "vector.h"

#include <assert.h>
#include <stdlib.h>

void vector_ptr_init(void **arr, size_t *size, size_t *capacity) {
    *arr = malloc(4 * sizeof(void*));
    *size = 0;
    *capacity = 8;
}

void vector_ptr_free(void **arr, size_t *size, size_t *capacity) {
    free(*arr);
    arr = NULL;
    *size = 0;
    *capacity = 0;
}

void vector_ptr_reserve(void **arr, size_t *size, size_t *capacity, size_t desired_size) {
    if (desired_size > *capacity) {
        while (1) {
            *capacity <<= 1; // multiply by 2
            if (*capacity >= desired_size) break;
        }

        void **new_alloc = malloc(*capacity * sizeof(void*));
        for (size_t i = 0; i < *size; i++) {
            new_alloc[i] = arr[i];
        }
        free(*arr);
        *arr = new_alloc;
    }
}

void vector_ptr_insert(void **arr, size_t *size, size_t *capacity, size_t index, void *item) {
    vector_ptr_reserve(arr, size, capacity, *size + 1);

    if (index == *size) {
        arr[*size + 1] = item;
    } else {
        assert(index < *size);

        for (size_t i = *size - 1; i > index; --i) {
            arr[i] = arr[i - 1];
        }

        arr[index] = item;
    }

    ++(*size);
}

void vector_ptr_remove(void **arr, size_t *size, size_t *capacity, size_t index) {
    assert(index < *size);
    for (size_t i = index; i < *size - 1; ++i) {
        arr[i] = arr[i+1];
    }

    --(*size);
}