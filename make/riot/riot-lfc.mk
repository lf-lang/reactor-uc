RIOT_MK_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

# Check if required environment variables exist
ifndef LF_MAIN
  $(error LF_MAIN is not defined. Please define it!)
endif

# Name of your RIOT application
APPLICATION ?= $(LF_MAIN)

# Path of generated lf c-code
LF_SRC_GEN_PATH ?= $(CURDIR)/src-gen/$(LF_MAIN)

# Only include generated files if build target is not "clean"
# In this case the src-gen folder was deleted
ifeq ($(MAKECMDGOALS),clean)
  # Delete src-gen folder if build target is "clean"
  _ :=  $(shell rm -rf $(LF_SRC_GEN_PATH))
else
  # Include the Makefile of the generated target application
  include $(LF_SRC_GEN_PATH)/Makefile

  # Include generated c files
  SRC += $(patsubst %, $(LF_SRC_GEN_PATH)/%, $(LFC_GEN_SOURCES))

  # Include generated main file
  SRC += $(LF_SRC_GEN_PATH)/${LFC_GEN_MAIN}

  # Include generated h files
  CFLAGS += -I$(LF_SRC_GEN_PATH)
endif

include $(RIOT_MK_DIR)/riot.mk
