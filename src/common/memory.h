/**
 * Project: IFJ26 / IAL compiler
 * File: memory.h
 * Authors:
 *   Adam Kolacek (xkolaca00)
 */

#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>

/**
 * Allocates memory of given size.
 * If allocation fails, prints an error to stderr and terminates with ERR_INTERNAL (99).
 *
 * @param size Number of bytes to allocate.
 * @return Pointer to allocated memory (never NULL).
 */
void *safe_malloc(size_t size);

/**
 * Allocates zero-initialized memory for an array of num elements of size bytes each.
 * If allocation fails, prints an error to stderr and terminates with ERR_INTERNAL (99).
 *
 * @param num Number of elements.
 * @param size Size of each element in bytes.
 * @return Pointer to allocated memory (never NULL).
 */
void *safe_calloc(size_t num, size_t size);

/**
 * Changes the size of the memory block pointed to by ptr.
 * If reallocation fails, prints an error to stderr and terminates with ERR_INTERNAL (99).
 *
 * @param ptr Pointer to previously allocated block (or NULL).
 * @param size New size in bytes.
 * @return Pointer to reallocated memory (never NULL).
 */
void *safe_realloc(void *ptr, size_t size);

/**
 * Duplicates a null-terminated string using safe_malloc.
 * Strictly portable across all C99 implementations.
 *
 * @param str String to duplicate.
 * @return Pointer to dynamically allocated copy of the string (or NULL if str is NULL).
 */
char *safe_strdup(const char *str);

#endif // MEMORY_H
