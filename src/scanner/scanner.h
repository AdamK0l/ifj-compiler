/**
 * Project: IFJ26 / IAL compiler
 * File: scanner.h
 * Authors:
 *   Bálint Marcell Šógor (xsogorb00)
 */

#ifndef SCANNER_H
#define SCANNER_H

#include "dstring.h"
#include "errors.h"
#include "token.h"
#include <stdio.h>

typedef struct {
    FILE *input;          // File with the source code
    int current;          // Current character
    int next;             // Next character
    int line;             // Line of current character
    int column;           // Column of current character
    DynamicString buffer; // Text we are reading
} Scanner;

// Initialises the scanner to read from input, returns ERR_OK or ERR_INTERNAL
ErrorCode scanner_init(Scanner *scanner, FILE *input);

#endif
