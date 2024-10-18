cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(reactor-uc-zephyr)

set(PLATFORM "ZEPHYR" CACHE STRING "Platform to target")
add_subdirectory(../../../../ reactor-uc)

target_sources(app PRIVATE src/receiver.c)
target_link_libraries(app PRIVATE reactor-uc)