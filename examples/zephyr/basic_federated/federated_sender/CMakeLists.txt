cmake_minimum_required(VERSION 3.20.0)
set(PLATFORM "ZEPHYR" CACHE STRING "Platform to target")
set(BUILD_EXAMPLES OFF CAHCHE BOOL)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(reactor-uc-zephyr)

add_subdirectory(../../../../ reactor-uc)

target_sources(app PRIVATE src/sender.c)
target_link_libraries(app PRIVATE reactor-uc)