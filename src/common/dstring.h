/**
 * Project: IFJ26 / IAL compiler
 * File: dstring.h
 * Authors:
 *   Adam Kolacek (xkolaca00)
 */

#ifndef DSTRING_H
#define DSTRING_H

#include <stddef.h>
#include <stdbool.h>

/**
 * Dynamically resizable string buffer with exponential capacity growth.
 * Always guarantees null-termination ('\0').
 */
typedef struct {
    char *data;         // Pointer to heap-allocated buffer
    size_t length;      // Current number of characters (excluding '\0')
    size_t capacity;    // Total allocated capacity in bytes
} DynamicString;

/**
 * Initializes an empty dynamic string with a default initial capacity.
 *
 * @param str Pointer to DynamicString structure.
 * @return true on success, false on allocation failure.
 */
bool dstring_init(DynamicString *str);

/**
 * Releases memory allocated by the dynamic string and resets its fields.
 */
void dstring_free(DynamicString *str);

/**
 * Clears the string content (length becomes 0, null-terminated) without freeing memory.
 * Highly efficient for buffer reuse across multiple tokens.
 */
void dstring_clear(DynamicString *str);

/**
 * Appends a single character to the dynamic string.
 *
 * @param str Pointer to DynamicString structure.
 * @param c Character to append.
 * @return true on success, false on allocation failure.
 */
bool dstring_append_char(DynamicString *str, char c);

/**
 * Appends a null-terminated C string to the dynamic string.
 *
 * @param str Pointer to DynamicString structure.
 * @param text Null-terminated string to append.
 * @return true on success, false on allocation failure.
 */
bool dstring_append_str(DynamicString *str, const char *text);

/**
 * Appends formatted text (printf-style) to the dynamic string.
 *
 * @param str Pointer to DynamicString structure.
 * @param format Printf-like format string.
 * @param ... Additional arguments.
 * @return true on success, false on formatting/allocation failure.
 */
bool dstring_append_format(DynamicString *str, const char *format, ...);

/**
 * Lexicographically compares two dynamic strings.
 *
 * @return Negative if a < b, 0 if equal, positive if a > b.
 */
int dstring_compare(const DynamicString *a, const DynamicString *b);

/**
 * Lexicographically compares a dynamic string with a null-terminated C string.
 *
 * @return Negative if a < text, 0 if equal, positive if a > text.
 */
int dstring_compare_str(const DynamicString *a, const char *text);

/**
 * Transfers ownership of the internal char buffer to the caller.
 * The dynamic string structure is reset and can be safely reinitialized or freed.
 *
 * @param str Pointer to DynamicString structure.
 * @return Pointer to dynamically allocated null-terminated string owned by caller.
 */
char *dstring_export(DynamicString *str);

#endif // DSTRING_H
