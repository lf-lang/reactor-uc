MODULE = reactor-uc

# Makefile.base defaults to `SRC ?= $(wildcard *.c)`, which does not recurse. Name
# the subdirectory explicitly.
SRC := $(wildcard *.c) $(wildcard network_channel/*.c)

include $(RIOTBASE)/Makefile.base