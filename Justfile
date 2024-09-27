# Test Target
test:
    cmake -B build
    cmake --build build
    make test -C build

# Clean Target
clean:
    rm -r build

# Coverage Target
coverage:
    cmake -B build
    cmake --build build
    make coverage -C build

# ASAN Target
asan:
    cmake -B build -DASAN=ON
    cmake --build build
    make test -C build

# Format Code
src_files := `find src -name '*.c'`
hdr_files := `find include -name '*.h'`
format:
    clang-format -i -style=file {{src_files}} {{hdr_files}}

# Format Check
format-check:
    clang-format --dry-run --Werror -style=file {{src_files}} {{hdr_files}}