.PHONY: clean test coverage asan format format-check ci lf-test lib proto

test: unit-test lf-test

# Generate protobuf code
proto:
	python3 external/nanopb/generator/nanopb_generator.py -Iexternal/nanopb/generator/proto/ -Iexternal/proto -L'#include "nanopb/%s"' -Dexternal/proto message.proto

# Build reactor-uc as a static library
lib:
	cmake -Bbuild 
	cmake --build build
	make -C build

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
SRC_FILES := $(shell find ./src -path ./src/generated -prune -o -name '*.c' -print)
HDR_FILES := $(shell find ./include -path ./include/reactor-uc/generated -prune -o -name '*.h' -print)

format:
	clang-format -i -style=file $(SRC_FILES) $(HDR_FILES)
	cd lfc && ./gradlew spotlessApply
	cd lfc && ./gradlew ktfmtFormat && ./gradlew spotlessApply && cd ..

# Check that the code base is formatted
format-check:
	clang-format --dry-run --Werror -style=file $(SRC_FILES) $(HDR_FILES)
	cd lfc && ./gradlew ktfmtCheck && ./gradlew spotlessCheck && cd ..

# Run the entire CI flow
ci: clean test coverage format-check

clean:
	rm -rf build test/lf/src-gen test/lf/bin
