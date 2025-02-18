# name of your application
APPLICATION = lf-coap-federated-receiver

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../../../../../../RIOT

# If no BOARD is found in the environment, use this default:
BOARD ?= native

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 1

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

# Enable reactor-uc features
CFLAGS += -DNETWORK_CHANNEL_COAP

CFLAGS += -DTHREAD_STACKSIZE_DEFAULT=10000
CFLAGS += -DTHREAD_STACKSIZE_MAIN=10000
CFLAGS += -DISR_STACKSIZE=10000

# Configure CoAP retransmission timeout
CFLAGS+= -DCONFIG_GCOAP_NO_RETRANS_BACKOFF=1
CFLAGS+= -DCONFIG_COAP_ACK_TIMEOUT_MS=400
CFLAGS+= -DCONFIG_COAP_MAX_RETRANSMIT=4

# Local IPv6 address
CFLAGS += -DCONFIG_GNRC_IPV6_STATIC_LLADDR='"fe80::cafe:cafe:cafe:2"'
CFLAGS += -DCONFIG_GNRC_IPV6_STATIC_LLADDR_IS_FIXED=1

# Remote Ipv6 address
CFLAGS += -DREMOTE_IP_ADDRESS='"fe80::cafe:cafe:cafe:1"'

include $(CURDIR)/../../../../../make/riot/riot.mk
