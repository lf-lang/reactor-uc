cmake_minimum_required(VERSION 3.20.0)
set(PLATFORM "PICO" CACHE STRING "Platform to target")
set(BUILD_EXAMPLES OFF CAHCHE BOOL)

if (DEFINED ENV{PICO_SDK_PATH})
  include("$ENV{PICO_SDK_PATH}/pico_sdk_init.cmake")
else()
  message(FATAL_ERROR "PICO_SDK_PATH environment variable not set")
endif()

if (DEFINED ENV{FREERTOS_KERNEL_PATH})
  include("$ENV{FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake")
else()
  message(FATAL_ERROR "FREERTOS_KERNEL_PATH environment variable not set")
endif()

if (NOT DEFINED ENV{FREERTOS_DEMO_PATH})
  message(FATAL_ERROR "FREERTOS_DEMO_PATH environment variable not set")
endif()
  
project(reactor-uc-pico)
pico_sdk_init()

add_executable(blinky src/blinky.c)

pico_enable_stdio_usb(blinky 1)
pico_enable_stdio_uart(blinky 0)
target_include_directories(blinky PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        $ENV{FREERTOS_DEMO_PATH}/Common/include)
        
# Set FreeRTOSConfig path for the reactor-uc library
set(FREERTOS_CONFIG_PATH ${CMAKE_CURRENT_LIST_DIR}/include)

add_subdirectory(../../../ reactor-uc)
target_link_libraries(blinky PRIVATE reactor-uc pico_stdlib pico_sync)

if (PICO_CYW43_SUPPORTED)
    target_link_libraries(blinky PRIVATE pico_cyw43_arch_none)
endif()
