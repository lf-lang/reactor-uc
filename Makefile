.PHONY: clean test coverage asan format format-check ci lf-test lib proto docs platform-test examples complexity

test: unit-test lf-test platform-test examples

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
	cd build && ctest --output-on-failure

# Build and run lf tests
lf-test:
	make -C test/lf

platform-test:
	cd test/platform && ./runAll.sh

examples:
	cd examples && ./runAll.sh

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
SRC_FILES := $(shell find ./src ./test/unit/ -path ./src/generated -prune -o -name '*.c' -print)
HDR_FILES := $(shell find ./include -path ./include/reactor-uc/generated -prune -o -name '*.h' -print)

format:
	clang-format -i -style=file $(SRC_FILES) $(HDR_FILES)
	cd lfc && ./gradlew ktfmtFormat && ./gradlew spotlessApply && cd ..

# Check that the code base is formatted
format-check:
	clang-format --dry-run --Werror -style=file $(SRC_FILES) $(HDR_FILES)
	cd lfc && ./gradlew ktfmtCheck && ./gradlew spotlessCheck && cd ..

# Run the entire CI flow
ci: clean format test coverage

clean:
	rm -rf build test/lf/src-gen test/lf/bin

complexity:
	complexity --histogram --score --thresh=2 $(SRC_FILES)

docs:
	mkdir -p doc/markdown/platform
	echo "\page platform-riot RIOT OS" > doc/markdown/platform/riot.md
	curl 'https://raw.githubusercontent.com/lf-lang/lf-riot-uc-template/refs/heads/main/README.md' >> doc/markdown/platform/riot.md
	echo "\page platform-zephyr Zephyr" > doc/markdown/platform/zephyr.md
	curl 'https://raw.githubusercontent.com/lf-lang/lf-zephyr-uc-template/refs/heads/main/README.md' >> doc/markdown/platform/zephyr.md
	echo "\page platform-pico Raspberry Pi Pico" > doc/markdown/platform/pico.md
	curl 'https://raw.githubusercontent.com/lf-lang/lf-pico-uc-template/refs/heads/main/README.md' >> doc/markdown/platform/pico.md
	doxygen
