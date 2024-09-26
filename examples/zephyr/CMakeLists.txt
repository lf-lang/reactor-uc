cmake_minimum_required(VERSION 3.20.0)

set(BUILD_TESTS OFF CACHE BOOL "Build unit tests")
set(BUILD_EXAMPLES OFF CACHE BOOL "Build example programs")
set(TEST_COVERAGE OFF CACHE BOOL "Compute test coverage")
set(ASAN OFF CACHE BOOL "Compile with AddressSanitizer")
set(PLATFORM "ZEPHYR" CACHE STRING "Platform to target")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(reactor-uc-zephyr)

add_subdirectory(../../ reactor-uc)

target_sources(app PRIVATE src/timer_ex.c)
target_link_libraries(app PRIVATE reactor-uc)