.PHONY: clean test coverage asan format format-check

test:
	cmake -Bbuild
	cmake --build build
	make test -C build

clean:
	rm -r build

coverage:
	cmake -Bbuild
	cmake --build build
	make coverage -C build

asan:
	cmake -Bbuild -DASAN=ON
	cmake --build build
	make test -C build

# This file lets you format the code-base with a single command.
FILES := $(shell find . -name '*.c' -o -name '*.h')
.PHONY: format
format:
	clang-format -i -style=file $(FILES)

.PHONY: format-check
format-check:
	clang-format --dry-run --Werror -style=file $(FILES)
