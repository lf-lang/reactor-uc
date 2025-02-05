# name of your application
APPLICATION = lf-uart-channel-test

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../../../../../RIOT

# If no BOARD is found in the environment, use this default:
BOARD ?= native

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 1

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

# Enable Unity
USEMODULE += Unity
USEMODULE += ztimer
USEMODULE += ztimer_sec

CFLAGS += -DLF_LOG_LEVEL_ALL=LF_LOG_LEVEL_DEBUG
CFLAGS += -DNETWORK_CHANNEL_UART
CFLAGS += -DLF_COLORIZE_LOGS=0
CFLAGS += -DLF_TIMESTAMP_LOGS=0

REACTION_QUEUE_SIZE = 32
EVENT_QUEUE_SIZE = 32

USEMODULE += periph_uart

CFLAGS += -DTHREAD_STACKSIZE_DEFAULT=10000
CFLAGS += -DTHREAD_STACKSIZE_MAIN=10000
CFLAGS += -DISR_STACKSIZE=10000

include $(CURDIR)/../../../../make/riot/riot.mk
