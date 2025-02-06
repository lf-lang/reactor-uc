LF_MAIN ?= CoapFederatedLF
LF_FED ?= r1

# ---- RIOT specific configuration ----
# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/RIOT

# If no BOARD is found in the environment, use this default:
BOARD ?= native

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 1

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

include $(REACTOR_UC_PATH)/make/riot/riot-lfc.mk