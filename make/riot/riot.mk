RIOT_MK_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(RIOT_MK_DIR)/../../../RIOT

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 1

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

# Use a peripheral timer for the delay, if available
FEATURES_OPTIONAL += periph_timer

# External modules
EXTERNAL_MODULE_DIRS += $(RIOT_MK_DIR)/external_modules
USEMODULE += reactor-uc



include $(RIOTBASE)/Makefile.include