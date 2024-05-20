cmake_minimum_required(VERSION 3.9)
project(reactor-uc LANGUAGES C)

option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_EXAMPLES "Build example programs" ON)
option(TEST_COVERAGE "Compute test coverage" ON)


find_program(CLANG_TIDY clang-tidy)
if (CLANG_TIDY)
  set(CMAKE_C_CLANG_TIDY clang-tidy; --header-filter=include/\(.*\)\\.h; --warnings-as-errors=*;)
else ()
  message(WARNING "Please install clang-tidy!")
endif()
file(GLOB SOURCES "src/*.c")
message(${SOURCES})
add_library(reactor-uc STATIC ${SOURCES})
target_compile_options(reactor-uc PRIVATE -Wall -Wextra -pedantic -Werror)
target_include_directories(reactor-uc PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

if(BUILD_TESTS)
  set(UNITY_DIR ${CMAKE_CURRENT_LIST_DIR}/external/Unity)
  include(CTest)
  add_library(Unity STATIC ${UNITY_DIR}/src/unity.c)
  target_include_directories(Unity PUBLIC ${UNITY_DIR}/src)
  set_target_properties( Unity PROPERTIES C_CLANG_TIDY "") # Disable clang-tidy for this external lib.
  add_subdirectory(test)
endif()

if(BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(TEST_COVERAGE)
  # FIXME: Integrate with lcov/gcov
endif()
