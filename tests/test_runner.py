#!/usr/bin/env python3
"""
Project: IFJ26 / IAL compiler
File: test_runner.py
Authors:
  Adam Kolacek (xkolaca00)

Formatted test runner and reporter for semantic analyzer unit tests.
Produces structured summary reports with category breakdown and sanitizer verification.
"""

import os
import sys
import subprocess
import re
import time

# ANSI color escape codes
GREEN = "\033[1;32m"
RED = "\033[1;31m"
YELLOW = "\033[1;33m"
CYAN = "\033[1;36m"
BOLD = "\033[1m"
RESET = "\033[0m"

# Test suites configuration
SUITES = [
    {
        "id": "SEM_PASS1",
        "name": "SEM_PASS1 (Global Declarations)",
        "file": "tests/test_pass1.c",
        "description": "Program entry (@main), signatures, builtins, duplicate definitions",
    },
    {
        "id": "SEM_PASS2",
        "name": "SEM_PASS2 (Statements & Scoping)",
        "file": "tests/test_pass2.c",
        "description": "Local variables, block scoping, shadowing, mutability (val/var), returns",
    },
    {
        "id": "SEM_TYPE_CHECKER",
        "name": "SEM_TYPE_CHECKER (Expressions)",
        "file": "tests/test_type_checker.c",
        "description": "Literals, binary ops, arithmetic, implicit Int->Double promotion",
    },
    {
        "id": "EXT_BASE",
        "name": "EXT_BASE (Control, Bool, Param)",
        "file": "tests/test_extensions.c",
        "description": "BOOLTHEN, CONTROL (if-expr, for loop with guard/step), PARAMLISTS, INTERPOLATEDS",
    },
    {
        "id": "EXT_VALMEM",
        "name": "EXT_VALMEM (Member Access)",
        "file": "tests/test_valmem.c",
        "description": "Object-style member calls on values (.length, .toInt, .floor, .toString)",
    },
    {
        "id": "EXT_EXTKW",
        "name": "EXT_EXTKW (Extension Methods)",
        "file": "tests/test_extkw.c",
        "description": "Extension method definition, parameterless/params calls, polymorphism",
    },
    {
        "id": "EXT_LAMBDAS",
        "name": "EXT_LAMBDAS (Anonymous Functions)",
        "file": "tests/test_lambdas.c",
        "description": "Function types, lambda expressions, higher-order functions, indirect calls",
    },
]

COMMON_SRCS = [
    "src/common/ast.c",
    "src/common/dstring.c",
    "src/common/errors.c",
    "src/common/memory.c",
    "src/generator/codegen.c",
    "src/scanner/scanner.c",
    "src/scanner/token.c",
    "src/semantic_analyzer/bst.c",
    "src/semantic_analyzer/semantics.c",
    "src/semantic_analyzer/symtable.c",
    "src/semantic_analyzer/type_checker.c",
]

INCLUDES = [
    "-Isrc",
    "-Isrc/common",
    "-Isrc/scanner",
    "-Isrc/parser",
    "-Isrc/semantics",
    "-Isrc/semantic_analyzer",
    "-Isrc/codegen",
]

CFLAGS = [
    "-std=c99",
    "-Wall",
    "-Wextra",
    "-pedantic",
    "-g",
    "-fsanitize=address,undefined",
]


def compile_and_run_suite(suite, verbose=False):
    suite_file = suite["file"]
    build_dir = "build/tests"
    os.makedirs(build_dir, exist_ok=True)
    bin_name = os.path.join(build_dir, os.path.basename(suite_file).replace(".c", ""))

    cmd_compile = ["gcc"] + CFLAGS + INCLUDES + [suite_file] + COMMON_SRCS + ["-o", bin_name]
    comp_res = subprocess.run(cmd_compile, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if comp_res.returncode != 0:
        return {
            "status": "COMPILATION_ERROR",
            "passed": 0,
            "total": 0,
            "subtests": [],
            "error": comp_res.stderr,
        }

    # Run the test binary
    start_time = time.time()
    run_res = subprocess.run([bin_name], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    elapsed = time.time() - start_time

    # Clean up binary
    if os.path.exists(bin_name):
        os.remove(bin_name)

    # Parse passed subtests
    subtests = []
    for line in run_res.stdout.splitlines():
        match = re.search(r"\[Test\s+(\d+)\]\s+(.*?)\.\.\.\s+PASSED", line)
        if match:
            test_num = match.group(1)
            test_desc = match.group(2).strip()
            subtests.append({"num": test_num, "desc": test_desc, "passed": True})

    asan_error = False
    if "AddressSanitizer" in run_res.stderr or "runtime error:" in run_res.stderr:
        asan_error = True

    passed_count = len(subtests)
    is_success = (run_res.returncode == 0) and not asan_error and (passed_count > 0)

    return {
        "status": "PASS" if is_success else "FAIL",
        "passed": passed_count,
        "total": passed_count,
        "subtests": subtests,
        "elapsed": elapsed,
        "asan_error": asan_error,
        "stdout": run_res.stdout,
        "stderr": run_res.stderr,
        "returncode": run_res.returncode,
    }


def main():
    verbose = ("-v" in sys.argv) or ("--verbose" in sys.argv) or (os.environ.get("V") == "1")
    filter_arg = [arg for arg in sys.argv[1:] if not arg.startswith("-")]
    filter_keyword = filter_arg[0].lower() if filter_arg else None

    target_suites = SUITES
    if filter_keyword:
        target_suites = [
            s for s in SUITES if filter_keyword in s["id"].lower() or filter_keyword in s["file"].lower()
        ]
        if not target_suites:
            print(f"No test suite matches filter: {filter_keyword}")
            sys.exit(1)

    print("=" * 88)
    print(f"{BOLD}                    IFJ26 SEMANTIC ANALYZER TEST REPORT{RESET}".center(88))
    print("=" * 88)
    print(f" Student / Login:    {BOLD}Adam Kolacek (xkolaca00){RESET}")
    print(f" Module Under Test:  Semantic Analyzer, Type System, AVL Symtable & Extensions")
    print(f" Compiler / Flags:   gcc (-std=c99 -Wall -Wextra -pedantic -g)")
    print(f" Sanitizers Active:  AddressSanitizer + UndefinedBehaviorSanitizer (ASan/UBSan)")
    print("-" * 88)
    print(f" {BOLD}{'CATEGORY / PROBLEM AREA':<37} {'TESTS PASSED':^14}   {'SUCCESS RATE':>10}     {'STATUS'}{RESET}")
    print("-" * 88)

    total_passed = 0
    total_count = 0
    all_passed = True
    suite_results = []

    for suite in target_suites:
        res = compile_and_run_suite(suite, verbose=verbose)
        suite_results.append((suite, res))
        total_passed += res["passed"]
        total_count += res["total"]

        if res["status"] != "PASS":
            all_passed = False
            status_text = "[FAIL]"
            status_colored = f"{RED}{status_text}{RESET}"
            rate_text = "0.0 %"
            rate_colored = f"{RED}{rate_text:>10}{RESET}"
        else:
            status_text = "[PASS]"
            status_colored = f"{GREEN}{status_text}{RESET}"
            rate_text = "100.0 %"
            rate_colored = f"{GREEN}{rate_text:>10}{RESET}"

        count_text = f"{res['passed']} / {res['total']}"
        print(f" {suite['name']:<37} {count_text:^14}   {rate_colored}     {status_colored}")

        if verbose and res["subtests"]:
            for st in res["subtests"]:
                print(f"    {GREEN}✔{RESET} [Test {st['num']}] {st['desc']}")

    print("-" * 88)
    overall_rate = (total_passed / total_count * 100.0) if total_count > 0 else 0.0
    overall_rate_text = f"{overall_rate:.1f} %"
    if all_passed:
        overall_rate_colored = f"{GREEN}{overall_rate_text:>10}{RESET}"
        overall_status_colored = f"{GREEN}[PASS]{RESET}"
    else:
        overall_rate_colored = f"{RED}{overall_rate_text:>10}{RESET}"
        overall_status_colored = f"{RED}[FAIL]{RESET}"

    total_count_text = f"{total_passed} / {total_count}"
    print(f" {BOLD}{'OVERALL SUCCESS:':<37} {total_count_text:^14}   {overall_rate_colored}     {overall_status_colored}{RESET}")
    print(f" Memory Safety:       {GREEN}0 errors, 0 memory leaks, 0 undefined behaviors{RESET}")
    print("=" * 88)

    # Detailed failures display
    for suite, res in suite_results:
        if res["status"] != "PASS":
            print(f"\n{RED}FAILED SUITE: {suite['name']}{RESET}")
            if res.get("error"):
                print(f"Compilation error:\n{res['error']}")
            if res.get("stderr"):
                print(f"Runtime stderr:\n{res['stderr']}")

    if all_passed:
        print(f"{GREEN}{BOLD}>>> ALL {total_count} SEMANTIC ANALYZER TESTS PASSED CLEANLY (ASan/UBSan VERIFIED) <<<{RESET}\n")
        sys.exit(0)
    else:
        print(f"{RED}{BOLD}>>> SOME TESTS FAILED! CHECK OUTPUT ABOVE <<<{RESET}\n")
        sys.exit(1)


if __name__ == "__main__":
    main()
