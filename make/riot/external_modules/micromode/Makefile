include $(RIOTBASE)/Makefile.base

# Set by the generated Makefile, which exports it for the sub-makes RIOT spawns here.
ifndef MICROMODE_PATH
  $(error MICROMODE_PATH is not defined. It is exported by the generated Makefile of a modal program.)
endif

SRC_DIR := $(MICROMODE_PATH)/src

all:
	$(QQ)"$(MAKE)" -C $(SRC_DIR) -f $(CURDIR)/micromode.mk
