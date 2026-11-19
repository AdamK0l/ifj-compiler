/* Parser test driver: parses the IFJ26 source on stdin and returns the error code. 
 * Parser test driver: parses the IFJ26 source on stdin and prints the result code.
 * Build and run all parser tests: bash tests/run_parser_tests.sh
 * */
#include "parser_internal.h"
#include <stdio.h>

int main(void) {
    Scanner scanner;
    if (scanner_init(&scanner, stdin) != ERR_OK) {
        return ERR_INTERNAL;
    }
    ErrorCode err = parser_run(&scanner);
    scanner_destroy(&scanner);
    printf("parser result: %d\n", err);
    return err;
}