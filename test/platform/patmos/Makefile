# Rules
all: build/HelloPatmos.elf

# Compiler and tools for PATMOS
CC = patmos-clang

# Paths
SRC_DIR = $(CURDIR)
BUILD_DIR = $(CURDIR)/build

# Source files
SOURCES = $(SRC_DIR)/HelloPatmos.c

# Compiler flags
CFLAGS = -O2 -Wall -Wextra -Werror

# Output binary
OUTPUT = $(BUILD_DIR)/HelloPatmos.elf

# Build rule
$(OUTPUT): $(SOURCES)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SOURCES) -o $(OUTPUT)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
