# This make file describes a package that can be used by the riot-os ecosystem.
#
# Documentation: 
# https://doc.riot-os.org/group__pkg.html
#

PKG_NAME=reactor-uc
PKG_URL=https://github.com/erlingrj/reactor-uc
PKG_VERSION=78cf8caba21d7547d22cc917c0a8d6fe1858e94b
PKG_LICENSE=BSD

CC:=gcc
AR:=ar
CFLAGS:=-Wall -Wextra -Wno-unused-parameter -O2 -g

# This file lets you format the code-base with a single command.
FILES := $(shell find . -name '*.c' -o -name '*.h')
CFILES := $(shell find ./src -name '*.c')

.PHONY: all clean format format-check

all: reactor-uc.a

$(TARGET): build-subdirs $(OBJS) find-all-objs
	@echo -e "\t" CXX $(CFLAGS) $(ALL_OBJS) -o $@
	@$(CC) $(CFLAGS) $(ALL_OBJS) -o $@

reactor-uc.a: $(TARGET)
	#$(CC) $(CFLAGS) -c $(CFILES) -I./include
	$(CC) $(CFLAGS) -C ./src/* -I./include
	$(AR) rcs reactor-uc.a ./*.o
	rm ./*.o

clean: 
	rm ./reactor-uc.a

format:
	clang-format -i -style=file $(FILES)

format-check:
	clang-format --dry-run --Werror -style=file $(FILES)


