cmake_minimum_required(VERSION 3.20.0)
project(reactor-uc-posix-hello)

set(PLATFORM "POSIX" CACHE STRING "Platform to target")
set(SCHEDULER "STATIC" CACHE STRING "Scheduler to use")
add_subdirectory(reactor-uc)

add_executable(app R1.c R2.c static_schedule.c Connections.c)
target_link_libraries(app PRIVATE reactor-uc)