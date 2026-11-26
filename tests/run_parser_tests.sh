#!/bin/bash
# Project: IFJ26 / IAL compiler
# Builds the parser test driver and runs the parser tests.
# Usage (from the repository root): bash tests/run_parser_tests.sh

gcc -std=c99 -Wall -Wextra -Isrc -Isrc/common -Isrc/scanner -Isrc/parser \
    tests/parser_test.c src/parser/*.c src/scanner/*.c src/common/*.c \
    -o parser_test || exit 1

echo "ok_prolog (expected 0):"
./parser_test < tests/ok_prolog.ifj

echo "bad_prolog (expected 2):"
./parser_test < tests/bad_prolog.ifj

echo "ok_parameters (expected 0):"
./parser_test < tests/ok_parameters.ifj

echo "bad_parameters (expected 2):"
./parser_test < tests/bad_parameters.ifj

echo "ok_factorial (expected 0):"
./parser_test < tests/ok_factorial.ifj
# AI-generated (Claude, Anthropic). Prompt (summary): "Add a loop that runs the parser on every extension example in tests/ext."
# Extension examples from the specification (all expected 0)
for f in tests/ext/*.ifj; do
    echo "$(basename "$f") (expected 0):"
    ./parser_test < "$f"
done
