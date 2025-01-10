# name of your application
APPLICATION = lf-coap-federated-sender

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

# Enable reactor-uc features
CFLAGS += -DNETWORK_CHANNEL_COAP_RIOT
REACTION_QUEUE_SIZE = 32
EVENT_QUEUE_SIZE = 32

CFLAGS += -DTHREAD_STACKSIZE_DEFAULT=10000
CFLAGS += -DTHREAD_STACKSIZE_MAIN=10000
CFLAGS += -DISR_STACKSIZE=10000

# Configure CoAP retransmission timeout
CFLAGS += -DCONFIG_GCOAP_NO_RETRANS_BACKOFF=1
CFLAGS += -DCONFIG_COAP_ACK_TIMEOUT_MS=400
CFLAGS += -DCONFIG_COAP_MAX_RETRANSMIT=4

# Check if ONLY_PRINT_IP is defined
# If ONLY_PRINT_IP is defined the REMOTE_ADDRESS is not needed
ifdef ONLY_PRINT_IP
  # ONLY_PRINT_IP is defined => Set CFLAGS for it
  CFLAGS += -DONLY_PRINT_IP=$(ONLY_PRINT_IP)
else ifdef REMOTE_ADDRESS
  # REMOTE_ADDRESS is defined => Set CFLAGS for it
  CFLAGS += -DREMOTE_ADDRESS=\"$(REMOTE_ADDRESS)\"
else
  # Neither is defined
  $(error Either define REMOTE_ADDRESS or set ONLY_PRINT_IP=1 to print the IP-Address of this device)
endif

include $(CURDIR)/../../../../make/riot/riot.mk
