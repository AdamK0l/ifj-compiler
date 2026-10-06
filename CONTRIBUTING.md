# Contributing Guidelines & Development Rules (IFJ26)

This document establishes the collaborative workflow, coding conventions, and engineering standards for our IFJ26 compiler project for courses IFJ and IAL at FIT BUT. Compliance with these rules is mandatory for all team members.

---

## 1. Git Workflow & Version Control

### Branching Strategy
- **Protected `main` branch:** Direct commits and pushes to `main` are prohibited.
- All development must take place in isolated, short-lived feature branches.
- Branch naming convention:
  - `feat/<short-name>` – New functionality (e.g., `feat/scanner-tokens`, `feat/symtable-bst`).
  - `fix/<short-name>` – Bug fixes (e.g., `fix/precedence-table-lookup`).
  - `test/<short-name>` – Test suite additions and test scripts.
  - `docs/<short-name>` – Documentation updates.

### Commit Guidelines
- **Atomic commits:** Each commit must address exactly one logical task or fix. Large bulk commits containing finished project blocks are strictly forbidden.
- **Compilability:** The project must successfully compile with `make` after every single commit.
- **Commit messages:** Follow the [Conventional Commits](https://www.conventionalcommits.org/) standard recommended in the project specification:
  - `feat(scanner): implement keyword detection`
  - `fix(parser): resolve expression grouping bug`
  - `refactor(symtable): encapsulate binary search tree balancing`
  - `chore(makefile): update compiler warning flags`

### Pull Requests & Code Review
- Merging into `main` requires an approved Pull Request (PR).
- Every PR must receive an **approval from at least one other team member** before being merged.
- PR reviewers are responsible for checking:
  1. Adherence to formatting and error-free compilation without compiler warnings.
  2. Memory safety via Valgrind.
  3. Proper AI attribution where applicable.

---

## 2. Coding Standards (C Language)

### Language & Naming Conventions
- **Language:** All identifiers, function names, type names, error messages, and comments must be written exclusively in English.
- **Variables & Parameters:** `snake_case` (e.g., `current_token`, `symtable_ptr`).
- **Functions:** `snake_case` prefixed by the respective module name (e.g., `scanner_get_next_token()`, `symtable_search()`, `parser_parse_program()`).
- **Data Types (`struct`, `enum`, `union`):** `PascalCase` (e.g., `Token`, `TokenType`, `SymtableNode`).
- **Constants & Enum values:** `UPPER_SNAKE_CASE` (e.g., `TOKEN_KEYWORD_DEF`, `ERR_SEM_TYPE`).
- **Private Helper Functions:** Mark with `static` and prefix with `_` (e.g., `static bool _is_whitespace(int c);`).

### Required File & Code Structure
- **Mandatory File Header:** Every single `.c` and `.h` source file must begin with a comment header stating the project name and the full names and logins of all contributing authors:
  ```c
  /**
   * Project: IFJ26 / IAL compiler
   * File: <filename>
   * Authors:
   *   Name Surname (xlogin01)
   *   Name Surname (xlogin02)
   */