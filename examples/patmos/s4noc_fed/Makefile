include $(REACTOR_UC_PATH)/make/patmos/patmos.mk

OUTPUT = s4noc_fed.elf
BIN_DIR = $(CURDIR)/bin

SOURCES += $(CURDIR)/main.c
SOURCES += $(CURDIR)/sender/build/sender.a 
SOURCES += $(CURDIR)/receiver/build/receiver.a

all: $(OUTPUT)

$(OUTPUT): $(SOURCES)
	@echo "BUILDING $(notdir $@) from $^"
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(SOURCES) -o $(BIN_DIR)/$(OUTPUT)

clean:
	rm -rf $(BIN_DIR)

.PHONY: all clean