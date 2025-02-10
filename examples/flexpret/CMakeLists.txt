cmake_minimum_required(VERSION 3.22)

if(NOT DEFINED ENV{FP_SDK_PATH})
  message(FATAL_ERROR "FP_SDK_PATH environment variable not set!")
endif()

set(PLATFORM "FLEXPRET" CACHE STRING "Set platform to FlexPRET")

include($ENV{FP_SDK_PATH}/cmake/riscv-toolchain.cmake)
include($ENV{FP_SDK_PATH}/cmake/fp-app.cmake)

project(fp-lf)

include(src-gen/Smoke/Include.cmake)
add_subdirectory(${REACTOR_UC_PATH})

add_executable(fp-smoke ${LFC_GEN_MAIN} ${LFC_GEN_SOURCES})
target_link_libraries(fp-smoke PUBLIC reactor-uc)
target_include_directories(fp-smoke PRIVATE ${LFC_GEN_INCLUDE_DIRS})

fp_add_outputs(fp-smoke)