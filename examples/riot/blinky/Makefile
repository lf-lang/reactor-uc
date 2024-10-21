# name of your application
APPLICATION = lf-test

# If no BOARD is found in the environment, use this default:
BOARD ?= native

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 1

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

# Enable networking
USEMODULE += netdev_default
USEMODULE += auto_init_gnrc_netif

# Enable sockets
USEMODULE += gnrc_ipv6_default
USEMODULE += sock_tcp
USEMODULE += posix_sockets
USEMODULE += posix_sleep
USEMODULE += posix_inet

# Enable posix threads
USEMODULE += pthread

include $(CURDIR)/../../../make/riot/riot.mk
