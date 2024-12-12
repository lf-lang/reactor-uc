RIOT_MK_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

# Include generated sources and makefiles if LF_MAIN is defined
ifdef LF_MAIN
# Execute the LF compiler
.PHONY: lfcg
lfcg:
	$(REACTOR_UC_PATH)/lfc/bin/lfc-dev src/$(LF_MAIN).lf

all: lfcg

# Check if REACTOR_UC_PATH variable exist
ifndef REACTOR_UC_PATH
$(error REACTOR_UC_PATH is not defined. Please define it!)
endif

# Name of your RIOT application
APPLICATION ?= $(LF_MAIN)

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/RIOT

# Path of generated lf c-code
LF_SRC_GEN_PATH ?= $(CURDIR)/src-gen/$(LF_MAIN)

# Include the Makefile of the generated target application
include $(LF_SRC_GEN_PATH)/Makefile

# Include generated c files
SRC += $(patsubst %, $(LF_SRC_GEN_PATH)/%, $(LFC_GEN_SOURCES))

# Include generated main file
SRC += $(LF_SRC_GEN_PATH)/${LFC_GEN_MAIN}

# Include generated h files
CFLAGS += -I$(LF_SRC_GEN_PATH)
endif

# Check if required environment variables exist
ifndef RIOTBASE
$(error RIOTBASE is not defined. Please define it!)
endif

ifndef EVENT_QUEUE_SIZE
$(error EVENT_QUEUE_SIZE is not defined. Please define it!)
endif

ifndef REACTION_QUEUE_SIZE
$(error REACTION_QUEUE_SIZE is not defined. Please define it!)
endif

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 1

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

# Use a peripheral timer for the delay, if available
FEATURES_OPTIONAL += periph_timer

# Include reactor-uc as an external module
EXTERNAL_MODULE_DIRS += $(RIOT_MK_DIR)/external_modules
USEMODULE += reactor-uc

# Apply project reactor-uc configuration variables
CFLAGS += -DEVENT_QUEUE_SIZE=$(EVENT_QUEUE_SIZE)
CFLAGS += -DREACTION_QUEUE_SIZE=$(REACTION_QUEUE_SIZE)

include $(RIOTBASE)/Makefile.include