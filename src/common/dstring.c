/**
 * Project: IFJ26 / IAL compiler
 * File: dstring.c
 * Authors:
 *   Adam Kolacek (xkolaca00)
 */

#include "dstring.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#define DSTRING_INIT_CAPACITY 16

/**
 * @brief Ensures buffer capacity is at least min_capacity by geometric doubling.
 *
 * [AI-Generated / AI-Assisted]
 * Tool: Google Gemini
 * Prompt: "Write an internal helper that geometrically resizes dynamic string buffer using realloc"
 *
 * @param str Pointer to DynamicString.
 * @param min_capacity Minimum required capacity in bytes.
 * @return True on success, false on allocation failure.
 */
static bool _dstring_ensure_capacity(DynamicString *str, size_t min_capacity) {
    if (str->capacity >= min_capacity) {
        return true;
    }

    size_t new_capacity = (str->capacity == 0) ? DSTRING_INIT_CAPACITY : str->capacity * 2;
    while (new_capacity < min_capacity) {
        new_capacity *= 2;
    }

    char *new_data = (char *)realloc(str->data, new_capacity);
    if (!new_data) {
        return false;
    }

    str->data = new_data;
    str->capacity = new_capacity;
    return true;
}

/**
 * @brief Initializes a dynamic string with default initial capacity.
 *
 * [AI-Generated / AI-Assisted]
 * Tool: Google Gemini
 * Prompt: "Implement dstring_init to allocate an empty null-terminated string buffer"
 *
 * @param str Pointer to uninitialized DynamicString.
 * @return True on success, false on allocation failure.
 */
bool dstring_init(DynamicString *str) {
    if (!str) {
        return false;
    }

    str->data = (char *)malloc(DSTRING_INIT_CAPACITY);
    if (!str->data) {
        str->length = 0;
        str->capacity = 0;
        return false;
    }

    str->data[0] = '\0';
    str->length = 0;
    str->capacity = DSTRING_INIT_CAPACITY;
    return true;
}

/**
 * @brief Deallocates the buffer and resets the string structure.
 *
 * @param str Pointer to DynamicString.
 */
void dstring_free(DynamicString *str) {
    if (!str) {
        return;
    }

    free(str->data);
    str->data = NULL;
    str->length = 0;
    str->capacity = 0;
}

/**
 * @brief Clears the contents without releasing the allocated buffer.
 *
 * @param str Pointer to DynamicString.
 */
void dstring_clear(DynamicString *str) {
    if (!str) {
        return;
    }

    if (str->data != NULL) {
        str->data[0] = '\0';
    }
    str->length = 0;
}

/**
 * @brief Appends a single character and maintains null termination.
 *
 * [AI-Generated / AI-Assisted]
 * Tool: Google Gemini
 * Prompt: "Write dstring_append_char to append a character with automatic buffer growth"
 *
 * @param str Pointer to DynamicString.
 * @param c Character to append.
 * @return True on success, false on memory allocation failure.
 */
bool dstring_append_char(DynamicString *str, char c) {
    if (!str) {
        return false;
    }

    // +2 for the new character and the null terminator
    if (!_dstring_ensure_capacity(str, str->length + 2)) {
        return false;
    }

    str->data[str->length] = c;
    str->length++;
    str->data[str->length] = '\0';
    return true;
}

/**
 * @brief Appends a null-terminated C string.
 *
 * [AI-Generated / AI-Assisted]
 * Tool: Google Gemini
 * Prompt: "Write dstring_append_str to append a string slice using memcpy"
 *
 * @param str Pointer to DynamicString.
 * @param text Null-terminated string to append.
 * @return True on success, false on allocation failure.
 */
bool dstring_append_str(DynamicString *str, const char *text) {
    if (!str || !text) {
        return false;
    }

    size_t text_len = strlen(text);
    if (!_dstring_ensure_capacity(str, str->length + text_len + 1)) {
        return false;
    }

    memcpy(str->data + str->length, text, text_len);
    str->length += text_len;
    str->data[str->length] = '\0';
    return true;
}

/**
 * @brief Appends printf-style formatted text using vsnprintf.
 *
 * [AI-Generated / AI-Assisted]
 * Tool: Google Gemini
 * Prompt: "Implement dstring_append_format using two-pass vsnprintf with va_copy"
 *
 * @param str Pointer to DynamicString.
 * @param format Printf format string.
 * @return True on success, false on formatting or allocation failure.
 */
bool dstring_append_format(DynamicString *str, const char *format, ...) {
    if (!str || !format) {
        return false;
    }

    va_list args;
    va_start(args, format);

    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, format, args);
    va_end(args);

    if (needed < 0) {
        va_end(args_copy);
        return false;
    }

    if (!_dstring_ensure_capacity(str, str->length + (size_t)needed + 1)) {
        va_end(args_copy);
        return false;
    }

    vsnprintf(str->data + str->length, (size_t)needed + 1, format, args_copy);
    va_end(args_copy);

    str->length += (size_t)needed;
    return true;
}

/**
 * @brief Compares two DynamicStrings lexicographically.
 *
 * @param a Pointer to first DynamicString.
 * @param b Pointer to second DynamicString.
 * @return 0 if equal, <0 if a < b, >0 if a > b.
 */
int dstring_compare(const DynamicString *a, const DynamicString *b) {
    const char *a_str = (a && a->data) ? a->data : "";
    const char *b_str = (b && b->data) ? b->data : "";
    return strcmp(a_str, b_str);
}

/**
 * @brief Compares DynamicString with a null-terminated C string.
 *
 * @param a Pointer to DynamicString.
 * @param text Null-terminated C string.
 * @return 0 if equal, <0 if a < text, >0 if a > text.
 */
int dstring_compare_str(const DynamicString *a, const char *text) {
    const char *a_str = (a && a->data) ? a->data : "";
    const char *b_str = (text != NULL) ? text : "";
    return strcmp(a_str, b_str);
}

/**
 * @brief Exports ownership of the internal heap buffer and resets the DynamicString.
 * The caller becomes responsible for freeing the returned char* buffer.
 *
 * [AI-Generated / AI-Assisted]
 * Tool: Google Gemini
 * Prompt: "Write dstring_export that returns the buffer and resets the struct to avoid copying"
 *
 * @param str Pointer to DynamicString.
 * @return Pointer to dynamically allocated null-terminated string.
 */
char *dstring_export(DynamicString *str) {
    if (!str) {
        return NULL;
    }

    char *result = str->data;
    if (!result) {
        result = (char *)malloc(1);
        if (result) {
            result[0] = '\0';
        }
    }

    str->data = NULL;
    str->length = 0;
    str->capacity = 0;

    return result;
}
