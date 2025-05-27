# Makefile for building a simple Hello World program for the Patmos platform
APP = main
# Compiler and tools for PATMOS
CC = patmos-clang

include $(CURDIR)/../../../../make/patmos/patmos.mk
# Paths
SRC_DIR = $(CURDIR)
BUILD_DIR = $(CURDIR)/build

# Source files
SOURCES += $(SRC_DIR)/$(APP).c 

CFLAGS += -O2 
CFLAGS += -DPLATFORM_PATMOS 
CFLAGS += -DNETWORK_CHANNEL_S4NOC 
CFLAGS += -DSCHEDULER_DYNAMIC
CFLAGS += -DFEDERATED

# Output binary
OUTPUT = $(BUILD_DIR)/$(APP).elf
all : $(OUTPUT)

# Build rule
$(OUTPUT): $(SOURCES)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SOURCES) -o $(OUTPUT)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
