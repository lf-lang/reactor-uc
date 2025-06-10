
# This Makefile is used to build the Patmos platform for the Reactor-UC project.
# It includes the necessary paths, sources, and compiler flags for the Patmos architecture.

# Compiler and tools for PATMOS
CC = patmos-clang

# Includes
CFLAGS += -I$(REACTOR_UC_PATH)/ 
CFLAGS += -I$(REACTOR_UC_PATH)/include 
CFLAGS += -I$(REACTOR_UC_PATH)/include/reactor-uc 
CFLAGS += -I$(REACTOR_UC_PATH)/include/reactor-uc/platform 
CFLAGS += -I$(REACTOR_UC_PATH)/include/reactor-uc/platform/patmos 
CFLAGS += -I$(REACTOR_UC_PATH)/external 

CFLAGS += -I$(REACTOR_UC_PATH)/external/nanopb 
CFLAGS += -I$(REACTOR_UC_PATH)/external/nanopb/pb

CFLAGS += -I$(REACTOR_UC_PATH)/external/proto


CFLAGS += -I$(REACTOR_UC_PATH)/external/Unity/src
CFLAGS += -I$(REACTOR_UC_PATH)/test/unit

UNITY_DIR = $(REACTOR_UC_PATH)/external/Unity
SOURCES += $(UNITY_DIR)/src/unity.c

SOURCES += $(wildcard $(REACTOR_UC_PATH)/src/*.c)

SOURCES += $(wildcard $(REACTOR_UC_PATH)/external/nanopb/*.c)
SOURCES += $(REACTOR_UC_PATH)/external/proto/message.pb.c

CFLAGS += -O2 
CFLAGS += -Wall -Wextra
CFLAGS += -DPLATFORM_PATMOS 
            