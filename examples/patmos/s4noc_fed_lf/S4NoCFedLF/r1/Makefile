LF_MAIN ?= S4NoCFedLF
LF_FED ?= r1

include $(REACTOR_UC_PATH)/make/patmos/patmos-lfc.mk

# ---- Patmos specific configuration ----
# Output directory
BIN_DIR = $(CURDIR)/bin
OUTPUT = $(BIN_DIR)/$(LF_MAIN)
all: $(OUTPUT)
 $(info Building $(LF_MAIN) federate $(LF_FED))
 $(info Output directory: $(BIN_DIR))
 $(info Output binary: $(OUTPUT))

# Build rule
$(OUTPUT): $(SOURCES)
	 mkdir -p $(BIN_DIR)
	$(CC) $(SOURCES) $(CFLAGS) -o $(OUTPUT)
# Clean rule
clean:
	 rm -rf $(BIN_DIR)