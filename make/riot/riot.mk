RIOT_MK_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

# Check if required environment variables exist
ifndef RIOTBASE
  $(error RIOTBASE is not defined. Please define it!)
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

# Configure CoAP retransmission timeout
# TODO: Specify generic keywords to share this configuration across platforms similar to EVENT_QUEUE_SIZE
CFLAGS += -DCONFIG_GCOAP_NO_RETRANS_BACKOFF=1
CFLAGS += -DCONFIG_COAP_ACK_TIMEOUT_MS=400
CFLAGS += -DCONFIG_COAP_MAX_RETRANSMIT=4

include $(RIOTBASE)/Makefile.include