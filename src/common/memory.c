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

/**
 * @brief Allocates memory of the requested size, terminating on allocation failure.
 * Ensures at least 1 byte is requested to prevent implementation-defined NULL on size 0.
 *
 * [AI-Generated / AI-Assisted]
 * Tool: Google Gemini
 * Prompt: "Write a safe malloc function that terminates with error code 99 if allocation fails"
 *
 * @param size Number of bytes to allocate.
 * @return Pointer to allocated memory (never NULL).
 */
void *safe_malloc(size_t size) {
    // If size is 0, allocate at least 1 byte to ensure unique non-null pointer where supported
    size_t alloc_size = (size == 0) ? 1 : size;
    void *ptr = malloc(alloc_size);
    if (!ptr) {
        error_exit_no_pos(ERR_INTERNAL, "Memory allocation failed: out of memory (requested %zu bytes)", size);
    }
    return ptr;
}

/**
 * @brief Allocates zero-initialized memory for an array of elements.
 *
 * [AI-Generated / AI-Assisted]
 * Tool: Google Gemini
 * Prompt: "Write a safe calloc function that terminates with ERR_INTERNAL on out-of-memory"
 *
 * @param num Number of elements.
 * @param size Size of each element in bytes.
 * @return Pointer to zero-initialized allocated memory (never NULL).
 */
void *safe_calloc(size_t num, size_t size) {
    size_t alloc_num = (num == 0) ? 1 : num;
    size_t alloc_size = (size == 0) ? 1 : size;
    void *ptr = calloc(alloc_num, alloc_size);
    if (!ptr) {
        error_exit_no_pos(ERR_INTERNAL, "Memory allocation failed: out of memory (requested %zu x %zu bytes)", num, size);
    }
    return ptr;
}

/**
 * @brief Reallocates memory block to a new size, terminating on failure.
 *
 * [AI-Generated / AI-Assisted]
 * Tool: Google Gemini
 * Prompt: "Write a safe realloc function that safely handles out-of-memory errors by exiting with code 99"
 *
 * @param ptr Pointer to previously allocated memory block (or NULL).
 * @param size New size in bytes.
 * @return Pointer to reallocated memory (never NULL).
 */
void *safe_realloc(void *ptr, size_t size) {
    size_t alloc_size = (size == 0) ? 1 : size;
    void *new_ptr = realloc(ptr, alloc_size);
    if (!new_ptr) {
        error_exit_no_pos(ERR_INTERNAL, "Memory reallocation failed: out of memory (requested %zu bytes)", size);
    }
    return new_ptr;
}

/**
 * @brief Portable C99 implementation of string duplication using safe_malloc.
 *
 * [AI-Generated / AI-Assisted]
 * Tool: Google Gemini
 * Prompt: "Write a safe_strdup function that duplicates a string using safe_malloc"
 *
 * @param str Input null-terminated string to duplicate.
 * @return Pointer to dynamically allocated duplicate (or NULL if str is NULL).
 */
char *safe_strdup(const char *str) {
    if (!str) {
        return NULL;
    }
    size_t len = strlen(str);
    char *copy = (char *)safe_malloc(len + 1);
    memcpy(copy, str, len + 1);
    return copy;
}
