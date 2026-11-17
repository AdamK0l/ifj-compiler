/**
 * Project: IFJ26 / IAL compiler
 * File: scanner.c
 * Authors:
 *   Bálint Marcell Šógor (xsogorb00)
 */

#include "scanner.h"

ErrorCode scanner_init(Scanner *scanner, FILE *input) {
    // Check function inputs
    if (scanner == NULL || input == NULL) {
        return ERR_INTERNAL;
    }

    // Feed the scanner the input and initialise line and column counters
    scanner->input = input;
    scanner->line = 1;
    scanner->column = 1;

    // Initialise lexem buffer, internal error if it fails
    if (!dstring_init(&scanner->buffer)) {
        return ERR_INTERNAL;
    }

    // Reading the first 2 characters
    scanner->current = fgetc(input);
    scanner->next = EOF;
    if (scanner->current != EOF) {
        scanner->next = fgetc(input);
    }

    return ERR_OK;
}