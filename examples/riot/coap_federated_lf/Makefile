REACTOR_UC_PATH ?= $(CURDIR)/../../../

# The name of the LF application inside "./src" to build/run/flash etc.
LF_MAIN ?= CoapFederatedLF

# Execute the LF compiler if build target is "all"
ifeq ($(firstword $(MAKECMDGOALS)),all)
  _ :=  $(shell $(REACTOR_UC_PATH)/lfc/bin/lfc-dev --gen-fed-templates src/$(LF_MAIN).lf)
endif

