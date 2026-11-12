cmake_minimum_required(VERSION 3.20.0)
set(EXTRA_CONF_FILE ${CMAKE_CURRENT_LIST_DIR}/../common.conf)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(lf-zephyr-coap-federated-receiver)

set(REACTOR_UC_PATH ${CMAKE_CURRENT_LIST_DIR}/../../../../..)

# Enable reactor-uc features
set(PLATFORM "ZEPHYR" CACHE STRING "Platform to target")
set(FEDERATED ON CACHE BOOL "")
zephyr_compile_definitions(NETWORK_CHANNEL_COAP)
add_subdirectory(${REACTOR_UC_PATH} reactor-uc)

target_sources(app PRIVATE src/main.c)
# Remote IPv4 address
target_compile_definitions(app PRIVATE REMOTE_IP_ADDRESS="192.0.2.1")
target_link_libraries(app PRIVATE reactor-uc)
