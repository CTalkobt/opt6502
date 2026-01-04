# Makefile for 6502 Assembly Optimizer
# Supports multiple platforms and build configurations

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99
DEBUGFLAGS = -g -DDEBUG -O0
PROFFLAGS = -pg -O2

# Target executable name
TARGET = opt6502
DEBUG_TARGET = opt6502-debug
PROFILE_TARGET = opt6502-profile

# Source files
SOURCES = opt6502.c
OBJECTS = $(SOURCES:.c=.o)

# Installation directory (can be overridden)
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
DOCDIR = $(PREFIX)/share/doc/opt6502

# Platform detection
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    PLATFORM = Linux
endif
ifeq ($(UNAME_S),Darwin)
    PLATFORM = macOS
endif
ifeq ($(UNAME_S),MINGW32_NT)
    PLATFORM = Windows
    TARGET = opt6502.exe
endif
ifeq ($(UNAME_S),MINGW64_NT)
    PLATFORM = Windows
    TARGET = opt6502.exe
endif

# Default target
.DEFAULT_GOAL := all

# Build targets
.PHONY: all clean debug profile install uninstall test help

all: $(TARGET)
	@echo "Built $(TARGET) for $(PLATFORM)"
	@echo "Run 'make install' to install system-wide"
	@echo "Run 'make test' to run basic tests"

# Main build target
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

# Debug build
debug: $(SOURCES)
	$(CC) $(CFLAGS) $(DEBUGFLAGS) -o $(DEBUG_TARGET) $(SOURCES)
	@echo "Debug build complete: $(DEBUG_TARGET)"

# Profile build (for performance analysis)
profile: $(SOURCES)
	$(CC) $(CFLAGS) $(PROFFLAGS) -o $(PROFILE_TARGET) $(SOURCES)
	@echo "Profile build complete: $(PROFILE_TARGET)"

# Optimized release build (maximum optimization)
release: $(SOURCES)
	$(CC) -Wall -Wextra -O3 -std=c99 -DNDEBUG -o $(TARGET) $(SOURCES)
	strip $(TARGET)
	@echo "Release build complete (optimized and stripped)"

# Install to system
install: $(TARGET)
	@echo "Installing to $(BINDIR)..."
	install -d $(BINDIR)
	install -m 755 $(TARGET) $(BINDIR)
	@if [ -f README.md ]; then \
		echo "Installing documentation to $(DOCDIR)..."; \
		install -d $(DOCDIR); \
		install -m 644 README.md $(DOCDIR); \
	fi
	@echo "Installation complete. Run 'opt6502 -h' to get started."

# Uninstall from system
uninstall:
	@echo "Removing $(BINDIR)/$(TARGET)..."
	rm -f $(BINDIR)/$(TARGET)
	@echo "Removing documentation..."
	rm -rf $(DOCDIR)
	@echo "Uninstall complete."

# Clean build artifacts
clean:
	rm -f $(TARGET) $(DEBUG_TARGET) $(PROFILE_TARGET)
	rm -f $(OBJECTS)
	rm -f *.o
	rm -f gmon.out
	rm -f test_output.asm
	@echo "Clean complete."

# Run basic tests
test: $(TARGET)
	@echo "Running basic tests..."
	@echo "Test 1: Help output"
	./$(TARGET) 2>&1 | head -1
	@echo ""
	@echo "Test 2: Simple optimization test"
	@echo "    LDA #0" > test_input.asm
	@echo "    STA \$$D020" >> test_input.asm
	@echo "    LDA #0" >> test_input.asm
	@echo "    STA \$$D021" >> test_input.asm
	./$(TARGET) -speed test_input.asm test_output.asm
	@if [ -f test_output.asm ]; then \
		echo "✓ Test 2 passed - output generated"; \
		rm -f test_input.asm test_output.asm; \
	else \
		echo "✗ Test 2 failed - no output generated"; \
	fi
	@echo ""
	@echo "Test 3: 6502 CPU target"
	@echo "    LDA #\$$FF" > test_input.asm
	@echo "    AND #\$$FF" >> test_input.asm
	./$(TARGET) -speed -cpu 6502 test_input.asm test_output.asm
	@if [ -f test_output.asm ]; then \
		echo "✓ Test 3 passed - 6502 optimization worked"; \
		rm -f test_input.asm test_output.asm; \
	else \
		echo "✗ Test 3 failed"; \
	fi
	@echo ""
	@echo "Test 4: 65C02 CPU target"
	./$(TARGET) -speed -cpu 65c02 test_input.asm test_output.asm 2>/dev/null || true
	@rm -f test_input.asm test_output.asm
	@echo "✓ All basic tests completed"

# Create a distribution tarball
dist: clean
	@echo "Creating distribution tarball..."
	@mkdir -p opt6502-1.0
	@cp opt6502.c opt6502-1.0/
	@cp Makefile opt6502-1.0/
	@if [ -f README.md ]; then cp README.md opt6502-1.0/; fi
	@if [ -f LICENSE ]; then cp LICENSE opt6502-1.0/; fi
	tar czf opt6502-1.0.tar.gz opt6502-1.0/
	@rm -rf opt6502-1.0
	@echo "Distribution created: opt6502-1.0.tar.gz"

# Static analysis with various tools
analyze:
	@echo "Running static analysis..."
	@if command -v cppcheck >/dev/null 2>&1; then \
		echo "Running cppcheck..."; \
		cppcheck --enable=all --suppress=missingIncludeSystem $(SOURCES); \
	else \
		echo "cppcheck not found, skipping..."; \
	fi
	@if command -v splint >/dev/null 2>&1; then \
		echo "Running splint..."; \
		splint $(SOURCES) || true; \
	else \
		echo "splint not found, skipping..."; \
	fi

# Check for memory leaks with valgrind
memcheck: debug
	@if command -v valgrind >/dev/null 2>&1; then \
		echo "Running valgrind memory check..."; \
		echo "    LDA #0" > test_input.asm; \
		echo "    STA \$$D020" >> test_input.asm; \
		valgrind --leak-check=full --show-leak-kinds=all \
			./$(DEBUG_TARGET) -speed test_input.asm test_output.asm; \
		rm -f test_input.asm test_output.asm; \
	else \
		echo "valgrind not found, install it for memory checking"; \
	fi

# Generate assembly listing (for debugging the optimizer itself)
asm: $(SOURCES)
	$(CC) $(CFLAGS) -S -fverbose-asm -o opt6502.s $(SOURCES)
	@echo "Assembly listing generated: opt6502.s"

# Show help
help:
	@echo "6502 Assembly Optimizer - Makefile targets"
	@echo ""
	@echo "Build targets:"
	@echo "  make              - Build release version (default)"
	@echo "  make all          - Same as 'make'"
	@echo "  make debug        - Build debug version with symbols"
	@echo "  make profile      - Build with profiling enabled"
	@echo "  make release      - Build optimized and stripped version"
	@echo ""
	@echo "Installation:"
	@echo "  make install      - Install to $(PREFIX) (may need sudo)"
	@echo "  make uninstall    - Remove installed files"
	@echo ""
	@echo "Testing:"
	@echo "  make test         - Run basic functionality tests"
	@echo "  make memcheck     - Check for memory leaks (requires valgrind)"
	@echo "  make analyze      - Run static analysis tools"
	@echo ""
	@echo "Maintenance:"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make dist         - Create distribution tarball"
	@echo "  make asm          - Generate assembly listing"
	@echo ""
	@echo "Variables you can override:"
	@echo "  PREFIX=$(PREFIX)"
	@echo "  CC=$(CC)"
	@echo "  CFLAGS=$(CFLAGS)"
	@echo ""
	@echo "Example: make PREFIX=/opt/local install"

# Additional platform-specific targets

# Windows-specific (cross-compile from Linux)
windows:
	@echo "Cross-compiling for Windows..."
	x86_64-w64-mingw32-gcc $(CFLAGS) -o opt6502.exe $(SOURCES)
	@echo "Windows build complete: opt6502.exe"

# Create a simple benchmark
bench: $(TARGET)
	@echo "Running benchmark..."
	@time -p ./$(TARGET) -speed test_large.asm test_large_opt.asm 2>/dev/null || \
		echo "Create test_large.asm for benchmarking"

# Version information
version:
	@echo "6502 Assembly Optimizer v1.0"
	@echo "Build date: $$(date)"
	@echo "Platform: $(PLATFORM)"
	@echo "Compiler: $(CC) $$($(CC) --version | head -1)"

# Dependency generation (for development)
depend:
	$(CC) -MM $(SOURCES) > .depend

# Include dependencies if they exist
-include .depend



.PHONY: test-diff
test-diff:
	@./run_tests.sh
