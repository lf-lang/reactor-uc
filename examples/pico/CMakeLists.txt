cmake_minimum_required(VERSION 3.20.0)
set(PLATFORM "PICO" CACHE STRING "Platform to target")
set(BUILD_EXAMPLES OFF CAHCHE BOOL)

if (DEFINED ENV{PICO_SDK_PATH})
  include("$ENV{PICO_SDK_PATH}/pico_sdk_init.cmake")
else()
  message(FATAL_ERROR "PICO_SDK_PATH environment variable not set")
endif()

project(reactor-uc-pico)
pico_sdk_init()
add_subdirectory(../../ reactor-uc)

add_executable(blinky blinky.c)

pico_enable_stdio_usb(blinky 1)
pico_enable_stdio_uart(blinky 0)
target_link_libraries(blinky PRIVATE reactor-uc)

if (PICO_CYW43_SUPPORTED)
    target_link_libraries(blinky PRIVATE pico_cyw43_arch_none)
endif()