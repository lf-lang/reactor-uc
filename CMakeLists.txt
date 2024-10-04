cmake_minimum_required(VERSION 3.9)
project(reactor-uc LANGUAGES C)

# Command line options for the build
set(BUILD_TESTS OFF CACHE BOOL "Build all tests")
set(BUILD_LF_TESTS OFF CACHE BOOL "Build lf tests")
set(BUILD_UNIT_TESTS OFF CACHE BOOL "Build unit tests")
set(TEST_COVERAGE OFF CACHE BOOL "Compute test coverage")
set(ASAN OFF CACHE BOOL "Compile with AddressSanitizer")
set(PLATFORM "POSIX" CACHE STRING "Platform to target")

set(EVENT_QUEUE_SIZE 10 CACHE STRING "Static size of the event queue")
set(REACTION_QUEUE_SIZE 10 CACHE STRING "Static size of the reaction queue")

# Code coverage setup 
if(TEST_COVERAGE)
  set(CMAKE_BUILD_TYPE "Debug")
  include(external/cmake/CodeCoverage.cmake)
  append_coverage_compiler_flags()
endif()

# Setup AddressSanitizer for chasing memory bugs.
if(ASAN)
  add_compile_options(-fsanitize=address)
  add_link_options(-fsanitize=address)
endif()

# Clang-tidy for static code analysis
if(BUILD_TESTS)
  set(BUILD_LF_TESTS ON)
  set(BUILD_UNIT_TESTS ON)
  set(CMAKE_BUILD_TYPE "Debug")
  find_program(CLANG_TIDY clang-tidy)
  if (CLANG_TIDY)
    set(CMAKE_C_CLANG_TIDY clang-tidy; --header-filter=include/\(.*\)\\.h; --warnings-as-errors=*;)
  else ()
    message(WARNING "Please install clang-tidy!")
  endif()
endif()

# TODO: Explicitly name all the sources.
file(GLOB SOURCES "src/*.c")
message(${SOURCES})

if (PLATFORM STREQUAL "POSIX")
  add_library(reactor-uc STATIC ${SOURCES})
  target_compile_definitions(reactor-uc PUBLIC PLATFORM_POSIX)
  target_link_libraries(reactor-uc PRIVATE pthread)
elseif (PLATFORM STREQUAL "ZEPHYR")
  zephyr_library_named(reactor-uc)
  zephyr_library_sources(${SOURCES})
  zephyr_library_link_libraries(kernel)
  add_compile_definitions(PLATFORM_ZEPHYR)
else ()
  message(FATAL_ERROR "No valid platform specified")
endif ()

target_compile_options(reactor-uc PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_compile_options (-fdiagnostics-color=always)
target_include_directories(reactor-uc PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/include/reactor-uc/config.h.in 
  ${CMAKE_CURRENT_SOURCE_DIR}/include/reactor-uc/config.h
)

if(BUILD_UNIT_TESTS)
  set(UNITY_DIR ${CMAKE_CURRENT_LIST_DIR}/external/Unity)
  include(CTest)
  add_library(Unity STATIC ${UNITY_DIR}/src/unity.c)
  target_include_directories(Unity PUBLIC ${UNITY_DIR}/src)
  set_target_properties( Unity PROPERTIES C_CLANG_TIDY "") # Disable clang-tidy for this external lib.
  add_subdirectory(test/unit)
endif()

if(BUILD_LF_TEST)
# TODO: Setup automatic code-gen and compiling of the LF tests as well.
# Ideally we include code-coverage from the LF tests into the overall coverage.
# Currently it is done from a stupid Makefile
endif()

