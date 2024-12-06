# name of your application
APPLICATION = HelloLF

# Path of generated lf c-code
SRC_GEN_PATH = $(CURDIR)/src-gen/$(APPLICATION)

# TODO: Can we call lfc here directly?

# If no BOARD is found in the environment, use this default:
BOARD ?= native

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 1

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

# Enable reactor-uc features
# CFLAGS += -DNETWORK_CHANNEL_TCP_POSIX

include $(SRC_GEN_PATH)/Makefile

SOURCES += $(LF_SOURCES) main.c
INCLUDE_DIRS += -I$(SRC_GEN_PATH)

include $(SRC_GEN_PATH)/reactor-uc/make/riot/riot.mk
