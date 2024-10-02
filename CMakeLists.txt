cmake_minimum_required(VERSION 3.9)
project(reactor-uc LANGUAGES C)

# Command line options for the build
set(BUILD_TESTS OFF CACHE BOOL "Build unit tests")
set(TEST_COVERAGE OFF CACHE BOOL "Compute test coverage")
set(ASAN OFF CACHE BOOL "Compile with AddressSanitizer")
set(PLATFORM "POSIX" CACHE STRING "Platform to target")

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
  set(CMAKE_BUILD_TYPE "Debug")
  find_program(CLANG_TIDY clang-tidy)
  if (CLANG_TIDY)
    set(CMAKE_C_CLANG_TIDY clang-tidy; --header-filter=include/\(.*\)\\.h; --warnings-as-errors)
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

add_compile_options (-fdiagnostics-color=always)
target_include_directories(reactor-uc PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

if(BUILD_TESTS)
  target_compile_options(reactor-uc PRIVATE -Wall -Wextra -pedantic -Werror)
  set(UNITY_DIR ${CMAKE_CURRENT_LIST_DIR}/external/Unity)
  include(CTest)
  add_library(Unity STATIC ${UNITY_DIR}/src/unity.c)
  target_include_directories(Unity PUBLIC ${UNITY_DIR}/src)
  set_target_properties( Unity PROPERTIES C_CLANG_TIDY "") # Disable clang-tidy for this external lib.
  add_subdirectory(test)
endif()

