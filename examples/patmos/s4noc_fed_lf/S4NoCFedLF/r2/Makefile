LF_MAIN ?= S4NoCFedLF
LF_FED ?= r2
include $(REACTOR_UC_PATH)/make/patmos/patmos-lfc.mk

# ---- Patmos specific configuration ----
# Output directory
BIN_DIR = $(CURDIR)/bin
OBJ_DIR = $(CURDIR)/obj
OUTPUT = $(BIN_DIR)/$(LF_MAIN).a
OBJECTS ?= $(SOURCES:.c=.bc)

all: $(OUTPUT)

# Build rule
$(OUTPUT): $(OBJECTS)
	mkdir -p $(BIN_DIR) 
	ar rcsv $@ $(OBJECTS);

# Compile to LLVM bitcode
%.bc: %.c
	mkdir -p $(OBJ_DIR)
	$(CC) -emit-llvm -c $^ -o $@ $(CFLAGS)

# Clean rule
clean:
	rm -rf $(BIN_DIR)