# name of your application
APPLICATION = lf-test

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
CFLAGS += -DEVENT_QUEUE_SIZE=32
CFLAGS += -DREACTION_QUEUE_SIZE=32
CFLAGS += -DSCHEDULER_DYNAMIC

include $(CURDIR)/../../../make/riot/riot.mk
