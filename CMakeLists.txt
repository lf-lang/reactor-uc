cmake_minimum_required(VERSION 3.9)

project(reactor-uc LANGUAGES C)
find_program(CLANG_TIDY clang-tidy)
if (CLANG_TIDY)
  set(CMAKE_C_CLANG_TIDY clang-tidy; -header-filter=reactor-uc/\(.*\)\\.h; -warnings-as-errors=*;)
else ()
  message(WARNING "Please install clang-tidy!")
endif()
file(GLOB SOURCES "src/*.c")
message(${SOURCES})
add_library(reactor-uc STATIC ${SOURCES})
target_compile_options(reactor-uc PRIVATE -Wall -Wextra -pedantic -Werror)

target_include_directories(reactor-uc PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

add_subdirectory(examples)
