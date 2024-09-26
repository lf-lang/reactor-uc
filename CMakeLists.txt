cmake_minimum_required(VERSION 3.9)
project(reactor-uc LANGUAGES C)

set(BUILD_TESTS ON CACHE BOOL "Build unit tests")
set(BUILD_EXAMPLES ON CACHE BOOL "Build example programs")
set(TEST_COVERAGE ON CACHE BOOL "Compute test coverage")
set(ASAN OFF CACHE BOOL "Compile with AddressSanitizer")
set(PLATFORM "POSIX" CACHE STRING "Platform to target")

# Code coverage setup 
if(TEST_COVERAGE)
  set(CMAKE_BUILD_TYPE "Debug")
  include(external/cmake/CodeCoverage.cmake)
  append_coverage_compiler_flags()
endif()

if(ASAN)
  add_compile_options(-fsanitize=address)
  add_link_options(-fsanitize=address)
endif()

# Clang-tidy setup
# find_program(CLANG_TIDY clang-tidy)
# if (CLANG_TIDY)
#   set(CMAKE_C_CLANG_TIDY clang-tidy; --header-filter=include/\(.*\)\\.h; --warnings-as-errors)
# else ()
#   message(WARNING "Please install clang-tidy!")
# endif()


file(GLOB SOURCES "src/*.c")
message(${SOURCES})

if (PLATFORM STREQUAL "POSIX")
  add_library(reactor-uc STATIC ${SOURCES})
  add_compile_definitions(PLATFORM_POSIX)
  target_link_libraries(reactor-uc PRIVATE pthread)
elseif (PLATFORM STREQUAL "ZEPHYR")
  zephyr_library_named(reactor-uc)
  zephyr_library_sources(${SOURCES})
  zephyr_library_link_libraries(kernel)
  add_compile_definitions(PLATFORM_ZEPHYR)
else ()
  message(FATAL_ERROR "No valid platform specified")
endif ()

target_compile_options(reactor-uc PRIVATE -Wall -Wextra -pedantic -Werror)
add_compile_options (-fdiagnostics-color=always)
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

