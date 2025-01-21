REACTOR_UC_PATH ?= $(CURDIR)/../../../

# The name of the LF application inside "./src" to build/run/flash etc.
LF_MAIN ?= CoapFederatedLF
FEDERATION ?= r1

# Execute the LF compiler if build target is "all"
ifeq ($(firstword $(MAKECMDGOALS)),all)
  _ :=  $(shell $(REACTOR_UC_PATH)/lfc/bin/lfc-dev src/$(LF_MAIN).lf)
endif

# ---- RIOT specific configuration ----
# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../../../../RIOT

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

# Configure CoAP retransmission timeout
CFLAGS += -DCONFIG_GCOAP_NO_RETRANS_BACKOFF=1
CFLAGS += -DCONFIG_COAP_ACK_TIMEOUT_MS=400
CFLAGS += -DCONFIG_COAP_MAX_RETRANSMIT=4

include $(REACTOR_UC_PATH)/make/riot/riot-lfc.mk
