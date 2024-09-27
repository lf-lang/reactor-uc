cmake_minimum_required(VERSION 3.20.0)
set(PLATFORM "POSIX" CACHE STRING "Platform to target")

project(timer-ex)

add_executable(timer_ex)
target_sources(timer_ex PRIVATE timer_ex.c)
add_subdirectory(../../ reactor-uc)
target_link_libraries(timer_ex PRIVATE reactor-uc)