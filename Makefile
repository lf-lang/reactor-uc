.PHONY: clean test coverage asan format format-check ci lf-test

test: unit-test lf-test

# Build and run the unit tests
unit-test:
	cmake -Bbuild -DBUILD_TESTS=ON
	cmake --build build
	make test -C build

# Build and run lf tests
lf-test:
	make -C test/lf

# Get coverage data on unit tests
coverage:
	cmake -Bbuild -DBUILD_TESTS=ON -DTEST_COVERAGE=ON
	cmake --build build
	make coverage -C build

# Compile tests with AddressSanitizer and run them
asan:
	cmake -Bbuild -DASAN=ON -DBUILD_TESTS=ON
	cmake --build build
	make test -C build

# Format the code base
SRC_FILES := $(shell find src -name '*.c')
HDR_FILES := $(shell find include -name '*.h')
format:
	clang-format -i -style=file $(SRC_FILES) $(HDR_FILES)

# Check that the code base is formatted
format-check:
	clang-format --dry-run --Werror -style=file $(SRC_FILES) $(HDR_FILES)


# Run the entire CI flow
ci: clean test coverage format-check


clean:
	rm -r build
