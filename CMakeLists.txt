cmake_minimum_required(VERSION 3.10)
project(reactor-uc LANGUAGES C)

# Command line options for the build
set(BUILD_TESTS OFF CACHE BOOL "Build all tests")
set(BUILD_LF_TESTS OFF CACHE BOOL "Build lf tests")
set(BUILD_UNIT_TESTS OFF CACHE BOOL "Build unit tests")
set(TEST_COVERAGE OFF CACHE BOOL "Compute test coverage")
set(ASAN OFF CACHE BOOL "Compile with AddressSanitizer")
set(PLATFORM "POSIX" CACHE STRING "Platform to target")
set(SCHEDULER "DYNAMIC" CACHE STRING "Scheduler to use")
set(NETWORK_CHANNEL_TCP_POSIX OFF CACHE BOOL "Use POSIX TCP NetworkChannel")
set(FEDERATED OFF CACHE BOOL "Compile with federated sources")

# Code coverage setup 
if(TEST_COVERAGE)
  set(CMAKE_BUILD_TYPE "Debug")
  include(external/cmake/CodeCoverage.cmake)
  append_coverage_compiler_flags()
  add_compile_options(-fprofile-update=atomic)
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
endif()

if(BUILD_UNIT_TESTS OR BUILD_LF_TESTS)
  set(NETWORK_CHANNEL_TCP_POSIX ON) # TODO: This is currently needed because one of the tests uses this stack, we need a nicer way of selecting build options for tests and apps.
  set(FEDERATED ON)
  set(LFC_RUNTIME_SYMLINK ON) 
  set(CMAKE_BUILD_TYPE "Debug")
  find_program(CLANG_TIDY clang-tidy)
  if (CLANG_TIDY)
    set(CMAKE_C_CLANG_TIDY clang-tidy; --header-filter=include/\(.*\)\\.h; --warnings-as-errors=*; -extra-arg=-Wno-unknown-warning-option)
  else ()
    message(WARNING "Please install clang-tidy!")
  endif()
endif()

file(GLOB SOURCES "src/*.c" "external/proto/*.c")
set(NANOPB_PATH external/nanopb)

if (PLATFORM STREQUAL "POSIX")
  add_library(reactor-uc STATIC ${SOURCES})
  target_link_libraries(reactor-uc PRIVATE pthread)
elseif (PLATFORM STREQUAL "FLEXPRET")
  add_library(reactor-uc STATIC ${SOURCES})
  add_subdirectory($ENV{FP_SDK_PATH} BINARY_DIR)
  target_link_libraries(reactor-uc PUBLIC fp-sdk)
elseif (PLATFORM STREQUAL "ZEPHYR")
  zephyr_library_named(reactor-uc)
  zephyr_library_sources(${SOURCES})
  zephyr_library_link_libraries(kernel)

  zephyr_library_named(nanopb)
  zephyr_library_sources(${NANOPB_PATH}/pb_common.c ${NANOPB_PATH}/pb_encode.c ${NANOPB_PATH}/pb_decode.c)
  target_link_libraries(reactor-uc PRIVATE nanopb)

elseif (PLATFORM STREQUAL "PICO")
  add_library(reactor-uc STATIC ${SOURCES})
  target_link_libraries(reactor-uc PUBLIC pico_stdlib pico_sync)

elseif (PLATFORM STREQUAL "PATMOS")
  add_library(reactor-uc STATIC ${SOURCES})
elseif( PLATFORM STREQUAL "FREERTOS")
  add_library(reactor-uc STATIC ${SOURCES})
  # Include FreeRTOSConfig directory 
  target_include_directories(reactor-uc PUBLIC ${FREERTOS_CONFIG_PATH})

  target_link_libraries(reactor-uc PUBLIC FreeRTOS-Kernel FreeRTOS-Kernel-Heap1)
elseif (PLATFORM STREQUAL "ESP_IDF")
  add_library(reactor-uc STATIC ${SOURCES})
  target_link_libraries(reactor-uc PUBLIC idf::freertos idf::spi_flash idf::esp_timer)
else ()
  message(FATAL_ERROR "No valid platform specified")
endif ()

# Add nanopb library. Note that for Zephyr it is built customly above.
if (NOT PLATFORM STREQUAL "ZEPHYR")
  add_library(nanopb ${NANOPB_PATH}/pb_common.c ${NANOPB_PATH}/pb_encode.c ${NANOPB_PATH}/pb_decode.c)
  set_target_properties(nanopb PROPERTIES C_CLANG_TIDY "") # Disable clang-tidy for this external lib.
  target_link_libraries(reactor-uc PRIVATE nanopb)
endif()

# Add compile definitions for platform and network channel specifics.
target_compile_definitions(reactor-uc PUBLIC "PLATFORM_${PLATFORM}")

# Add compile definition for scheduler used
target_compile_definitions(reactor-uc PRIVATE "SCHEDULER_${SCHEDULER}")

if(NETWORK_CHANNEL_TCP_POSIX)
  target_compile_definitions(reactor-uc PRIVATE NETWORK_CHANNEL_TCP_POSIX)
endif()

if(FEDERATED)
  target_compile_definitions(reactor-uc PUBLIC FEDERATED)
endif()

target_compile_options(reactor-uc PRIVATE -Wall -Wextra -Werror)

# Disable selected warnings 
if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
  target_compile_options(reactor-uc PRIVATE -Wno-zero-length-bounds)
endif()

add_compile_options (-fdiagnostics-color=always)
target_include_directories(reactor-uc PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include ${CMAKE_CURRENT_LIST_DIR}/external)

if(BUILD_UNIT_TESTS OR BUILD_LF_TESTS)
  # Global property to collect coverage dependencies across all test scopes.
  define_property(GLOBAL PROPERTY LF_COVERAGE_DEPS
    BRIEF_DOCS "Targets needed for coverage"
    FULL_DOCS "List of test/build targets that coverage depends on")
  set_property(GLOBAL PROPERTY LF_COVERAGE_DEPS "")

  set(UNITY_DIR ${CMAKE_CURRENT_LIST_DIR}/external/Unity)
  include(CTest)
  add_library(Unity STATIC ${UNITY_DIR}/src/unity.c)
  target_include_directories(Unity PUBLIC ${UNITY_DIR}/src)
  set_target_properties(Unity PROPERTIES C_CLANG_TIDY "") # Disable clang-tidy for this external lib.
endif()

if(BUILD_UNIT_TESTS)
  add_subdirectory(test/unit)
endif()

if(BUILD_LF_TESTS)
  include(cmake/lfc.cmake)
  include(test/lf/lf_tests.cmake)
endif()

if(TEST_COVERAGE)
  get_property(COVERAGE_DEPS GLOBAL PROPERTY LF_COVERAGE_DEPS)
  setup_target_for_coverage_lcov(
    NAME coverage
    EXECUTABLE ctest
      EXCLUDE
        "external/**"
        "test/**"
        "src-gen/**"
      LCOV_ARGS --rc lcov_branch_coverage=1
      GENHTML_ARGS --rc lcov_branch_coverage=1
      DEPENDENCIES ${COVERAGE_DEPS})
endif()
