cmake_minimum_required(VERSION 3.20.0)
project(reactor-uc-posix-hello)

set(PLATFORM "POSIX" CACHE STRING "Platform to target")
add_subdirectory(../../../ reactor-uc)

add_executable(app hello.c)
target_link_libraries(app PRIVATE reactor-uc)