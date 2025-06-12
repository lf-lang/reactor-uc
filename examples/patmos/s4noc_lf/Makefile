REACTOR_UC_PATH ?= $(CURDIR)/../../../
LF_MAIN ?= s4noc

# The name of the LF application inside "./src" to build/run/flash etc.
SRC_GEN_PATH ?= ./src-gen/$(LF_MAIN)

# Execute the LF compiler if build target is "all"
ifeq ($(firstword $(MAKECMDGOALS)),all)
  _ :=  $(shell $(REACTOR_UC_PATH)/lfc/bin/lfc-dev src/$(LF_MAIN).lf)
endif

# ---- Patmos specific configuration ----
include ./src-gen/$(LF_MAIN)/Makefile
include $(REACTOR_UC_PATH)/make/patmos/patmos-lfc.mk 

CFLAGS += -DSCHEDULER_DYNAMIC 
CFLAGS += -DEVENT_QUEUE_SIZE=$(EVENT_QUEUE_SIZE) 
CFLAGS += -DREACTION_QUEUE_SIZE=$(REACTION_QUEUE_SIZE) 

# Output directory
BIN_DIR = $(CURDIR)/bin
OUTPUT = $(BIN_DIR)/$(LF_MAIN)

all: $(OUTPUT)

# Build rule
$(OUTPUT): $(SOURCES)
	mkdir -p $(BIN_DIR)
	$(CC) $(SOURCES) $(CFLAGS) -o $(OUTPUT)

clean:
	rm -rf $(BIN_DIR) ./src-gen

.PHONY: all clean

