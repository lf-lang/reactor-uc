cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(HelloLF)

set(PLATFORM "ZEPHYR" CACHE STRING "Set platform to Zephyr")
include(src-gen/HelloLF/CMakeLists.txt)
add_subdirectory(${REACTOR_UC_PATH})

target_sources(app PRIVATE main.c ${LFC_GEN_SOURCES})
target_link_libraries(app PRIVATE reactor-uc)
target_include_directories(app PRIVATE ${LF_INCLUDE_DIRS})
