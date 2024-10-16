cmake_minimum_required(VERSION 3.22)

if(DEFINED ENV{FP_SDK_PATH})
  # Set toolchain to RISC-V; must be done before call to project()
  include($ENV{FP_SDK_PATH}/cmake/riscv-toolchain.cmake)
else()
  message(FATAL_ERROR "FP_SDK_PATH environment variable not set!")
endif()

project(fp-timer
  LANGUAGES C
  DESCRIPTION "LF on FlexPRET using uC runtime running a simple timer"
  VERSION 1.0.0
)

set(DEBUG true)
set(SUPPORT_LONG_LONG true) # printf

add_executable(fp-timer timer.c)

set(PLATFORM "FLEXPRET" CACHE STRING "Platform to target")
set(BUILD_EXAMPLES OFF CAHCHE BOOL)

add_subdirectory(../.. reactor-uc)
#add_subdirectory($ENV{FP_SDK_PATH} BINARY_DIR)
#target_link_libraries(fp-lf-uc PRIVATE
#    ${CMAKE_CURRENT_LIST_DIR}/../src-gen/FlexPRET_uC/build/libFlexPRET_uC.a
#    ${CMAKE_CURRENT_LIST_DIR}/../src-gen/FlexPRET_uC/build/reactor-uc/libreactor-uc.a
#)


target_link_libraries(fp-timer PRIVATE fp-sdk reactor-uc)

include($ENV{FP_SDK_PATH}/cmake/fp-app.cmake)

fp_add_outputs(fp-timer)



# cmake_minimum_required(VERSION 3.20.0)
# set(PLATFORM "PICO" CACHE STRING "Platform to target")
# set(BUILD_EXAMPLES OFF CAHCHE BOOL)

# if (DEFINED ENV{PICO_SDK_PATH})
#   include("$ENV{PICO_SDK_PATH}/pico_sdk_init.cmake")
# else()
#   message(FATAL_ERROR "PICO_SDK_PATH environment variable not set")
# endif()

# project(reactor-uc-pico)
# pico_sdk_init()
# add_subdirectory(../../ reactor-uc)

# add_executable(hello_world
#    timer_ex.c
# )

# target_link_libraries(hello_world PRIVATE reactor-uc)