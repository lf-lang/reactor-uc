cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(lf-zephyr-coap-channel-test)

set(REACTOR_UC_PATH ${CMAKE_CURRENT_LIST_DIR}/../../../..)

# Enable reactor-uc features
set(PLATFORM "ZEPHYR" CACHE STRING "Platform to target")
set(FEDERATED ON CACHE BOOL "")
zephyr_compile_definitions(NETWORK_CHANNEL_COAP)
add_subdirectory(${REACTOR_UC_PATH} reactor-uc)

# Unity
target_sources(app PRIVATE src/main.c ${REACTOR_UC_PATH}/external/Unity/src/unity.c)
target_include_directories(app PRIVATE ${REACTOR_UC_PATH}/external/Unity/src)
target_link_libraries(app PRIVATE reactor-uc)
