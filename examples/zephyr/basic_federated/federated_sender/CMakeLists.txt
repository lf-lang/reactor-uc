cmake_minimum_required(VERSION 3.20.0)

set(BOARD frdm_k64f)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(reactor-uc-zephyr)

# TODO: Improve the way of pickng which Network channels to add to the build.
set(NETWORK_POSIX_TCP ON CACHE BOOL "blah")
set(PLATFORM "ZEPHYR" CACHE STRING "Platform to target")
add_subdirectory(../../../../ reactor-uc)

target_sources(app PRIVATE src/sender.c)
target_link_libraries(app PRIVATE reactor-uc)