# Project: IFJ26 / IAL compiler
# File: Makefile
# Authors:
#   Adam Kolacek (xkolaca00)

CC ?= gcc
CFLAGS ?= -std=c99 -Wall -Wextra -pedantic -g
LDFLAGS ?=

# Header search paths for modular development
INCLUDES = -Isrc -Isrc/common -Isrc/scanner -Isrc/parser -Isrc/semantics -Isrc/semantic_analyzer -Isrc/codegen

# Automatic dependency tracking (.d files)
DEPFLAGS = -MMD -MP

ALL_CFLAGS = $(CFLAGS) $(INCLUDES) $(DEPFLAGS)

# Output binary executable name
TARGET = ifj26

# Source files (supports both nested development subdirectories and flat src/ for submission)
SRCS = $(wildcard src/*.c) $(wildcard src/*/*.c)
OBJS = $(patsubst %.c, build/%.o, $(SRCS))
DEPS = $(OBJS:.o=.d)

# Student login for submission archive naming
LOGIN = xkolaca00
ARCHIVE = $(LOGIN).zip
PACK_DIR = _pack

.PHONY: all clean format valgrind pack

# Default target: build executable
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Compile .c to .o inside build/ directory
build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(ALL_CFLAGS) -c $< -o $@

# Include auto-generated header dependency rules
-include $(DEPS)

# Clean up generated artifacts
clean:
	rm -rf build $(TARGET) $(PACK_DIR) $(ARCHIVE)

# Format codebase according to .clang-format
format:
	@which clang-format > /dev/null 2>&1 || (echo "clang-format is not installed" && exit 1)
	clang-format -i $(shell find src -type f \( -name '*.c' -o -name '*.h' \))

# Run the binary under Valgrind
valgrind: $(TARGET)
	valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all ./$(TARGET)

# Package project into submission ZIP archive (flattening sources into src/ as required by IFJ rules)
pack: clean
	@echo "==> Checking for duplicate file names before flattening..."
	@DUPS=$$(find src -type f \( -name '*.c' -o -name '*.h' \) -printf '%f\n' | sort | uniq -d); \
	if [ -n "$$DUPS" ]; then \
		echo "ERROR: Duplicate file names detected across subdirectories:"; \
		echo "$$DUPS"; \
		exit 1; \
	fi
	@echo "==> Preparing submission package $(ARCHIVE)..."
	rm -rf $(PACK_DIR) $(ARCHIVE)
	mkdir -p $(PACK_DIR)/src
	@# 1. Copy all .c and .h files directly into _pack/src (flattening)
	find src -type f \( -name '*.c' -o -name '*.h' \) -exec cp {} $(PACK_DIR)/src/ \;
	@# 2. Copy mandatory and accompanying root files
	cp Makefile $(PACK_DIR)/
	[ -f rozdeleni ] && cp rozdeleni $(PACK_DIR)/ || true
	[ -f rozsireni ] && cp rozsireni $(PACK_DIR)/ || true
	[ -f dokumentace.pdf ] && cp dokumentace.pdf $(PACK_DIR)/ || true
	[ -d tests ] && cp -r tests $(PACK_DIR)/ || true
	@# 3. Sanity check: verify that flattened sources compile cleanly in _pack/
	@echo "==> Verifying build of flattened project..."
	$(MAKE) -C $(PACK_DIR) clean
	$(MAKE) -C $(PACK_DIR)
	$(MAKE) -C $(PACK_DIR) clean
	@# 4. Create ZIP archive
	@echo "==> Creating ZIP archive..."
	find $(PACK_DIR) -name '.*' -delete
	cd $(PACK_DIR) && zip -r ../$(ARCHIVE) .
	rm -rf $(PACK_DIR)
	@# 5. Run official submission check script if present
	@if [ -f is_it_ok.sh ]; then \
		echo "==> Running official check script is_it_ok.sh..."; \
		chmod +x is_it_ok.sh && ./is_it_ok.sh $(ARCHIVE); \
	else \
		echo "==> Note: is_it_ok.sh not found in root (download it from course e-learning for final verification)."; \
	fi
	@echo "==> Done: Archive $(ARCHIVE) has been successfully created and verified."
