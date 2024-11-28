cmake_minimum_required(VERSION 3.22)

project(patmos)

add_executable(patmos src/main.c)
add_subdirectory(src-gen/Smoke)
target_link_libraries(patmos PUBLIC Smoke)
