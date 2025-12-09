include $(REACTOR_UC_PATH)/make/patmos/patmos.mk

# Allow slower startup handshakes on Patmos by extending the transient wait window.
# Escape parentheses to satisfy /bin/sh invocation of patmos-clang.
CFLAGS += -DTRANSIENT_WAIT_TIME=MSEC\(2000\)

FED = receiver

# Paths
SRC_DIR = $(CURDIR)
BUILD_DIR = $(CURDIR)/build

# Source files
SOURCES = $(FED).c

OBJECTS ?= $(SOURCES:.c=.bc)

# Output binary
OUTPUT = $(FED).a

all: $(OUTPUT)

# Build rule
$(OUTPUT): $(OBJECTS) 
	@echo "BUILDING $(notdir $@) from $^"
	mkdir -p $(BUILD_DIR)
	llvm-ar  rcsv $(BUILD_DIR)/$(OUTPUT) $^;

%.bc: %.c
	@echo "$(notdir $^) COMPILED TO $(notdir $@)"
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -emit-llvm -c $^ -o $@ 

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(FED).bc

.PHONY: all clean