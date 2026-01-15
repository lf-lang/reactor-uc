cmake_minimum_required(VERSION 3.20.0)
set(BOARD frdm_k64f)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
set(NETWORK_CHANNEL_TCP_POSIX ON CACHE BOOL "")
set(PLATFORM "ZEPHYR" CACHE STRING "Platform to target")
set(FEDERATED ON CACHE BOOL "If this program is federate")
set(FEDERATE "nxp1" CACHE STRING "Federate to build")

project(reactor-uc-zephyr)

if (NOT DEFINED LF_MAIN)
  set(LF_MAIN "Primefinder")
endif()

if (NOT DEFINED LOG_LEVEL)
  set(LOG_LEVEL "LF_LOG_LEVEL_WARN")
endif()

set(LF_MAIN_TARGET app)

include($ENV{REACTOR_UC_PATH}/cmake/lfc.cmake)

lf_setup()
lf_run_lfc(${CMAKE_CURRENT_SOURCE_DIR}/src ${LF_MAIN})
lf_build_generated_code(${LF_MAIN_TARGET} ${CMAKE_CURRENT_SOURCE_DIR}/src-gen/${LF_MAIN}/${FEDERATE})
