cmake_minimum_required(VERSION 3.20.0)
project(reactor-uc-posix-hello)

set(PLATFORM "POSIX" CACHE STRING "")
set(NETWORK_POSIX_TCP ON CACHE STRING "")
add_subdirectory(../../../ reactor-uc)

add_executable(sender sender.c)
target_link_libraries(sender PRIVATE reactor-uc)

add_executable(receiver receiver.c)
target_link_libraries(receiver PRIVATE reactor-uc)