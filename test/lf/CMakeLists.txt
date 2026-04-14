cmake_minimum_required(VERSION 3.20)
project(LFTests LANGUAGES C)

# Reactor-UC path: either from env or passed via -D
if(NOT DEFINED REACTOR_UC_PATH)
  set(REACTOR_UC_PATH $ENV{REACTOR_UC_PATH})
endif()

if(NOT REACTOR_UC_PATH)
  message(FATAL_ERROR "REACTOR_UC_PATH must be set (env var or -DREACTOR_UC_PATH=...)")
endif()

include(CTest)
set(LFC_RUNTIME_SYMLINK ON CACHE BOOL "Use a symlink to the runtime")
include(${REACTOR_UC_PATH}/cmake/lfc.cmake)

set(PLATFORM "POSIX" CACHE STRING "Target platform")
set(TEST_COVERAGE OFF CACHE BOOL "Compute test coverage")

# Code coverage setup
if(TEST_COVERAGE)
  set(CMAKE_BUILD_TYPE "Debug")
  include(${REACTOR_UC_PATH}/external/cmake/CodeCoverage.cmake)
  append_coverage_compiler_flags()
endif()

# Build runtime ONCE
add_compile_definitions(FEDERATED NETWORK_CHANNEL_TCP_POSIX)
add_subdirectory(${REACTOR_UC_PATH} reactor-uc-build)
target_compile_definitions(reactor-uc PUBLIC LF_LOG_LEVEL_ALL=LF_LOG_LEVEL_WARN)

# Discover and register all LF tests
include(${CMAKE_CURRENT_LIST_DIR}/lf_tests.cmake)
