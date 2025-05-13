REACTOR_UC_PATH ?= $(CURDIR)/../../../
LF_MAIN ?= HelloLF

# The name of the LF application inside "./src" to build/run/flash etc.
SRC_GEN_PATH ?= ./src-gen/$(LF_MAIN)

# Execute the LF compiler if build target is "all"
ifeq ($(firstword $(MAKECMDGOALS)),all)
  _ :=  $(shell $(REACTOR_UC_PATH)/lfc/bin/lfc-dev src/$(LF_MAIN).lf)
endif

# ---- Patmos specific configuration ----
include ./src-gen/$(LF_MAIN)/Makefile

SRC = $(REACTOR_UC_PATH)/src/*.c
SRC += $(patsubst %, $(SRC_GEN_PATH)/%, $(LFC_GEN_SOURCES) $(LFC_GEN_MAIN)) 

INC = -I$(REACTOR_UC_PATH)/include -I$(REACTOR_UC_PATH)/external -I$(SRC_GEN_PATH)
FLG = -DPLATFORM_PATMOS -DSCHEDULER_DYNAMIC -DEVENT_QUEUE_SIZE=$(EVENT_QUEUE_SIZE) -DREACTION_QUEUE_SIZE=$(REACTION_QUEUE_SIZE) -O2

# Output directory
BIN_DIR = $(CURDIR)/bin
OUTPUT = $(BIN_DIR)/$(LF_MAIN)

all: $(OUTPUT)

# Build rule
$(OUTPUT): $(SRC)
	mkdir -p $(BIN_DIR)
	patmos-clang $(SRC) $(INC) $(FLG) -o $(OUTPUT) -Wno-everything

clean:
	rm -rf $(BIN_DIR) ./src-gen

.PHONY: all clean

