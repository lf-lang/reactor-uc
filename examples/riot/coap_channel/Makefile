# name of your application
APPLICATION = lf-federated-coap-channel

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
CFLAGS += -DNETWORK_CHANNEL_COAP_RIOT
CFLAGS += -DEVENT_QUEUE_SIZE=32
CFLAGS += -DREACTION_QUEUE_SIZE=32

USEMODULE += shell_cmds_default
USEMODULE += ps
USEMODULE += shell
USEMODULE += gnrc_icmpv6_echo
USEMODULE += ipv4_addr
USEMODULE += shell_cmd_gnrc_udp

include $(CURDIR)/../../../make/riot/riot.mk
