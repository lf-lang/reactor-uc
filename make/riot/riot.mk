RIOT_MK_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

# Include generated sources and makefiles if SRC_GEN_PATH is defined
ifdef SRC_GEN_PATH
include $(SRC_GEN_PATH)/Makefile

# Include generated c files
SRC += $(patsubst %, $(SRC_GEN_PATH)/%, $(LFC_GEN_SOURCES))

# Include generated main file
SRC += $(SRC_GEN_PATH)/${LFC_GEN_MAIN}

# Include generated h files
CFLAGS += -I$(SRC_GEN_PATH)
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