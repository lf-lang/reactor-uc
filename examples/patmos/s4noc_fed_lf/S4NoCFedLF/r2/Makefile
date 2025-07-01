LF_MAIN ?= S4NoCFedLF
LF_FED ?= r2

include $(REACTOR_UC_PATH)/make/patmos/patmos-lfc.mk

# ---- Patmos specific configuration ----
# Output directory
BIN_DIR = $(CURDIR)/bin
OBJ_DIR = $(CURDIR)/obj
OUTPUT = $(BIN_DIR)/$(LF_MAIN).a
# OBJECTS = $(patsubst %.c,$(OBJ_DIR)/%.o,$(SOURCES))
OBJECTS ?= $(SOURCES:.c=.bc)
FILTER_OUT ?= ""

all: $(OUTPUT)

# Build rule
$(OUTPUT): $(OBJECTS)
	@echo "BUILDING $(notdir $@) from $^ except $(FILTER_OUT)"
	mkdir -p $(BIN_DIR) 
	llvm-ar rcsv $@ $(filter-out $(FILTER_OUT), $(OBJECTS));

%.bc: %.c
	@echo "$(notdir $^) COMPILED TO $(notdir $@)"
	mkdir -p $(OBJ_DIR)
	$(CC) -emit-llvm -c $^ -o $@ $(CFLAGS)

# Clean rule
clean:
	 rm -rf $(BIN_DIR)