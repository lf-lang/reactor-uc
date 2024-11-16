cmake_minimum_required(VERSION 3.22)

if(NOT DEFINED ENV{FP_SDK_PATH})
  message(FATAL_ERROR "FP_SDK_PATH environment variable not set!")
endif()

include($ENV{FP_SDK_PATH}/cmake/riscv-toolchain.cmake)
include($ENV{FP_SDK_PATH}/cmake/fp-app.cmake)

project(fp-lf)

# add_executable(fp-hello src/main.c)
# add_subdirectory(src-gen/HelloFlexPRET)
# target_link_libraries(fp-hello PUBLIC HelloFlexPRET)
# fp_add_outputs(fp-hello)


add_executable(fp-smoke src/main.c)
add_subdirectory(src-gen/Smoke)
target_link_libraries(fp-smoke PUBLIC Smoke)
fp_add_outputs(fp-smoke)