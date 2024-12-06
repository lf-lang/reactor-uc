# name of your application
APPLICATION = HelloLF

# find better name? SRC_GEN_PATH maybe
APPLICATION_PATH = $(CURDIR)/src-gen/$(APPLICATION)

# TODO: Can we call lfc here directly?

# If no BOARD is found in the environment, use this default:
BOARD ?= native

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 1

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

include $(APPLICATION_PATH)/Makefile

SOURCES += $(LF_SOURCES) main.c
INCLUDE_DIRS += -I$(APPLICATION_PATH)

# Enable reactor-uc features
# CFLAGS += -DNETWORK_CHANNEL_TCP_POSIX
CFLAGS += -DEVENT_QUEUE_SIZE=$(EVENT_QUEUE_SIZE)
CFLAGS += -DREACTION_QUEUE_SIZE=$(REACTION_QUEUE_SIZE)

include $(APPLICATION_PATH)/reactor-uc/make/riot/riot.mk
