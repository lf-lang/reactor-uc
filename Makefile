.PHONY: clean test coverage asan format format-check

test:
	cmake -Bbuild -DBUILD_TESTS=ON
	cmake --build build
	make test -C build

clean:
	rm -r build

coverage:
	cmake -Bbuild -DBUILD_TESTS=ON -DTEST_COVERAGE=ON
	cmake --build build
	make coverage -C build

asan:
	cmake -Bbuild -DASAN=ON -DBUILD_TESTS=ON
	cmake --build build
	make test -C build

# This file lets you format the code-base with a single command.
SRC_FILES := $(shell find src -name '*.c')
HDR_FILES := $(shell find include -name '*.h')
.PHONY: format
format:
	clang-format -i -style=file $(SRC_FILES) $(HDR_FILES)

.PHONY: format-check
format-check:
	clang-format --dry-run --Werror -style=file $(SRC_FILES) $(HDR_FILES)
