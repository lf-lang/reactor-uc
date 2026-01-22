cmake_minimum_required(VERSION 3.10)
project(reactor-uc LANGUAGES C)

# Command line options for the build
set(BUILD_TESTS OFF CACHE BOOL "Build all tests")
set(BUILD_LF_TESTS OFF CACHE BOOL "Build lf tests")
set(BUILD_UNIT_TESTS OFF CACHE BOOL "Build unit tests")
set(TEST_COVERAGE OFF CACHE BOOL "Compute test coverage")
set(ASAN OFF CACHE BOOL "Compile with AddressSanitizer")
set(OTEL OFF CACHE BOOL "Compile with AddressSanitizer")
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
  set(NETWORK_CHANNEL_TCP_POSIX ON) # TODO: This is currently needed because one of the tests uses this stack, we need a nicer way of selecting build options for tests and apps.
  set(FEDERATED ON)
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

if (OTEL)
  add_subdirectory(external/opentelemetry-cpp)
  SET(ZLIB_LIBRARY ZLIB_LIBRARY /nix/store/srby6wmvg7dp454pwb6qvaxdiri38sc1-zlib-1.3.1/lib/)
  SET(ZLIB_INCLUDE_DIR /nix/store/srby6wmvg7dp454pwb6qvaxdiri38sc1-zlib-1.3.1/include)
  SET(WITH_STL ON)
  include(external/opentelemetry-cpp/opentelemetry-cpp.cmake)
  add_opentelemetry()
  add_subdirectory(external/opentelemetry-c)
  target_link_libraries(reactor-uc PUBLIC opentelemetry-c)
  target_include_directories(reactor-uc PUBLIC external/opentelemetry-c/include)
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

if(BUILD_UNIT_TESTS)
  set(UNITY_DIR ${CMAKE_CURRENT_LIST_DIR}/external/Unity)
  include(CTest)
  add_library(Unity STATIC ${UNITY_DIR}/src/unity.c)
  target_include_directories(Unity PUBLIC ${UNITY_DIR}/src)
  set_target_properties( Unity PROPERTIES C_CLANG_TIDY "") # Disable clang-tidy for this external lib.
  add_subdirectory(test/unit)
endif()
