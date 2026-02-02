include $(REACTOR_UC_PATH)/make/patmos/patmos.mk

# Allow slower startup handshakes on Patmos by extending the transient wait window.
CFLAGS += -DTRANSIENT_WAIT_TIME=MSEC\(2000\)

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