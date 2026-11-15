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

void dstring_free(DynamicString *str) {
    if (!str) {
        return;
    }

    free(str->data);
    str->data = NULL;
    str->length = 0;
    str->capacity = 0;
}

void dstring_clear(DynamicString *str) {
    if (!str) {
        return;
    }

    if (str->data != NULL) {
        str->data[0] = '\0';
    }
    str->length = 0;
}

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

int dstring_compare(const DynamicString *a, const DynamicString *b) {
    const char *a_str = (a && a->data) ? a->data : "";
    const char *b_str = (b && b->data) ? b->data : "";
    return strcmp(a_str, b_str);
}

int dstring_compare_str(const DynamicString *a, const char *text) {
    const char *a_str = (a && a->data) ? a->data : "";
    const char *b_str = (text != NULL) ? text : "";
    return strcmp(a_str, b_str);
}

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
