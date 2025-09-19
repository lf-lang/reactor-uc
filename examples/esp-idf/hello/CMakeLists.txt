cmake_minimum_required(VERSION 3.20.0)
set(PLATFORM "ESP_IDF" CACHE STRING "Platform to target")
set(BUILD_EXAMPLES OFF CAHCHE BOOL)
set(TIMEOUT "1" ON CACHE STRING "Timeout in seconds, disable for infinite")

if(TIMEOUT)
    add_compile_definitions(TIMEOUT=${TIMEOUT})
else()
    add_compile_definitions(TIMEOUT_FOREVER)
endif()

set(IDF_COMPONENTS "freertos" "esptool_py")

if(DEFINED ENV{IDF_PATH})
else()
    message(FATAL_ERROR "IDF_PATH environment variable not set")
endif()

# Include for ESP-IDF build system functions
include($ENV{IDF_PATH}/tools/cmake/idf.cmake)

project(hello)

# Create idf::{target} and idf::freertos static libraries
idf_build_process(${IDF_TARGET} 
                COMPONENTS ${IDF_COMPONENTS} 
                SDKCONFIG ${CMAKE_CURRENT_LIST_DIR}/sdkconfig
                BUILD_DIR ${CMAKE_BINARY_DIR})

add_subdirectory(../../../ reactor-uc)

# suppress unused parameter warnings
target_compile_options(reactor-uc PRIVATE -Wno-unused-parameter)

set(elf_file ${CMAKE_PROJECT_NAME}.elf)
add_executable(${elf_file} main.c)
target_link_libraries(${elf_file} PRIVATE reactor-uc)

idf_build_executable(${elf_file})