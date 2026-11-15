/**
 * Project: IFJ26 / IAL compiler
 * File: memory.c
 * Authors:
 *   Adam Kolacek (xkolaca00)
 */

#include "memory.h"
#include "errors.h"
#include <stdlib.h>
#include <string.h>

void *safe_malloc(size_t size) {
    // If size is 0, allocate at least 1 byte to ensure unique non-null pointer where supported
    size_t alloc_size = (size == 0) ? 1 : size;
    void *ptr = malloc(alloc_size);
    if (!ptr) {
        error_exit_no_pos(ERR_INTERNAL, "Memory allocation failed: out of memory (requested %zu bytes)", size);
    }
    return ptr;
}

void *safe_calloc(size_t num, size_t size) {
    size_t alloc_num = (num == 0) ? 1 : num;
    size_t alloc_size = (size == 0) ? 1 : size;
    void *ptr = calloc(alloc_num, alloc_size);
    if (!ptr) {
        error_exit_no_pos(ERR_INTERNAL, "Memory allocation failed: out of memory (requested %zu x %zu bytes)", num, size);
    }
    return ptr;
}

void *safe_realloc(void *ptr, size_t size) {
    size_t alloc_size = (size == 0) ? 1 : size;
    void *new_ptr = realloc(ptr, alloc_size);
    if (!new_ptr) {
        error_exit_no_pos(ERR_INTERNAL, "Memory reallocation failed: out of memory (requested %zu bytes)", size);
    }
    return new_ptr;
}

char *safe_strdup(const char *str) {
    if (!str) {
        return NULL;
    }
    size_t len = strlen(str);
    char *copy = (char *)safe_malloc(len + 1);
    memcpy(copy, str, len + 1);
    return copy;
}
