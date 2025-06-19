PATMOS_MK_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

# Check if required environment variables exist
ifndef LF_MAIN
  $(error LF_MAIN is not defined. Please define it!)
endif

# Check if this is a federated program
ifdef LF_FED
  $(info Building federated program: $(LF_MAIN) with federation: $(LF_FED))
  # Name of your application
  APPLICATION ?= $(LF_MAIN)-$(LF_FED)

  # Path of generated lf c-code
  LF_SRC_GEN_PATH ?= $(CURDIR)/src-gen/$(LF_MAIN)/$(LF_FED)
else
  $(info Building non-federated program: $(LF_MAIN))
  # Name of your application
  APPLICATION ?= $(LF_MAIN)

  # Path of generated lf c-code
  LF_SRC_GEN_PATH ?= $(CURDIR)/src-gen/$(LF_MAIN)
endif

# Only include generated files if build target is not "clean"
# In this case the src-gen folder was deleted
ifeq ($(firstword $(MAKECMDGOALS)),clean)
  # Delete src-gen folder if build target is "clean"
  _ :=  $(shell rm -rf $(LF_SRC_GEN_PATH))
else
  # Include the Makefile of the generated target application
  include $(LF_SRC_GEN_PATH)/Makefile

  # Include generated c files
  SOURCES += $(patsubst %, $(LF_SRC_GEN_PATH)/%, $(LFC_GEN_SOURCES))

  # Include generated main file
  SOURCES += $(LF_SRC_GEN_PATH)/${LFC_GEN_MAIN}

  # Include generated h files
  CFLAGS += -I$(LF_SRC_GEN_PATH)

  # Add compile definitions produced by the LF compiler
  CFLAGS += $(patsubst %, -D%, $(LFC_GEN_COMPILE_DEFS))

  include $(PATMOS_MK_DIR)/patmos.mk
endif

