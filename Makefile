# Makefile for mach-c
# Compiler and flags
CC = clang
CFLAGS = -std=c23 -Wall -Wextra -Werror -pedantic -O2
CFLAGS_DEBUG = -std=c23 -Wall -Wextra -Werror -pedantic -g -O0 -DDEBUG

# LLVM flags
LLVM_CFLAGS = $(shell llvm-config --cflags)
LLVM_LDFLAGS = $(shell llvm-config --ldflags --libs core)

LDFLAGS = $(LLVM_LDFLAGS) 

# Directories
SRCDIR = src
INCDIR = include
BINDIR = bin
OBJDIR = $(BINDIR)/obj

# Target executable
TARGET = $(BINDIR)/cmach

# Source files
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
HEADERS = $(wildcard $(INCDIR)/*.h)

# Include path
INCLUDES = -I$(INCDIR)

# Default target
all: $(TARGET)

# Create directories
$(BINDIR):
	mkdir -p $(BINDIR)

$(OBJDIR): | $(BINDIR)
	mkdir -p $(OBJDIR)

# Build object files
$(OBJDIR)/%.o: $(SRCDIR)/%.c $(HEADERS) | $(OBJDIR)
	$(CC) $(CFLAGS) $(LLVM_CFLAGS) $(INCLUDES) -c $< -o $@

# Link executable
$(TARGET): $(OBJECTS) | $(BINDIR)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

# Debug build
debug: CFLAGS = $(CFLAGS_DEBUG)
debug: LLVM_CFLAGS = $(shell llvm-config --cflags)
debug: $(TARGET)

# Clean build artifacts
clean:
	rm -rf $(BINDIR)

# Install (copy to /usr/local/bin)
install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

# Uninstall
uninstall:
	rm -f /usr/local/bin/cmach

# Run tests (placeholder)
test: $(TARGET)
	@echo "No tests defined yet"

# Show help
help:
	@echo "Available targets:"
	@echo "  all       - Build the project (default)"
	@echo "  debug     - Build with debug flags"
	@echo "  clean     - Remove build artifacts"
	@echo "  install   - Install to /usr/local/bin"
	@echo "  uninstall - Remove from /usr/local/bin"
	@echo "  test      - Run tests"
	@echo "  help      - Show this help"

# Phony targets
.PHONY: all debug clean install uninstall test help

# Dependencies
-include $(OBJECTS:.o=.d)

# Generate dependency files
$(OBJDIR)/%.d: $(SRCDIR)/%.c | $(OBJDIR)
	@$(CC) $(CFLAGS) $(LLVM_CFLAGS) $(INCLUDES) -MM $< | sed 's,\($*\)\.o[ :]*,$(OBJDIR)/\1.o $@ : ,g' > $@
