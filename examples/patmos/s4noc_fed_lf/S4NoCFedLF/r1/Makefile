LF_MAIN ?= S4NoCFedLF
LF_FED ?= r1

include $(REACTOR_UC_PATH)/make/patmos/patmos-lfc.mk

# ---- Patmos specific configuration ----
# Output directory
BIN_DIR = $(CURDIR)/bin
OBJ_DIR = $(CURDIR)/obj
OUTPUT = $(BIN_DIR)/$(LF_MAIN).a
# OBJECTS = $(patsubst %.c,$(OBJ_DIR)/%.o,$(SOURCES))
OBJECTS = $(SOURCES:.c=.bc)

all: $(OUTPUT)
	$(info Building $(LF_MAIN) federate $(LF_FED))
	$(info Output directory: $(BIN_DIR))
	$(info Output binary: $(OUTPUT))
	$(info Sources: $(SOURCES))
	$(info Compiler: $(CC))
	$(info Compiler flags: $(CFLAGS))

# Build rule
$(OUTPUT): $(OBJECTS)
	mkdir -p $(BIN_DIR)
	$(CC) -c $(SOURCES) $(CFLAGS)
	llvm-ar rcs $@ $(OBJECTS) 

%.bc: %.c
	mkdir -p $(OBJ_DIR)
	$(CC) -emit-llvm -c $< -o $@ $(CFLAGS)

# Clean rule
clean:
	 rm -rf $(BIN_DIR)