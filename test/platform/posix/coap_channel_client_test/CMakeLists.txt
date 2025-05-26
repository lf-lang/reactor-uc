cmake_minimum_required(VERSION 3.20.0)
project(reactor-uc-posix-coap-channel-test)

set(PLATFORM "POSIX" CACHE STRING "Platform to target")
add_subdirectory(../../../../ reactor-uc)

add_executable(app main.c)

# LIBCOAP START
find_library(LIBCOAP_LIBRARIES coap-3)
find_path(LIBCOAP_INCLUDE_DIRS coap3/coap.h)

target_include_directories(app PRIVATE ${LIBCOAP_INCLUDE_DIRS})
target_link_libraries(app PRIVATE ${LIBCOAP_LIBRARIES})
# LIBCOAP END

target_link_libraries(app PRIVATE reactor-uc)