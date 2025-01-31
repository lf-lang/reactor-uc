cmake_minimum_required(VERSION 3.20.0)

set(LF_MAIN FederatedConnection)
set(LF_MAIN_TARGET r1)
set(PROJECT_ROOT /home/erling/dev/reactor-uc/test/lf/FederatedConnection/..)
set(FEDERATE r1)
project(FederatedConnection_FederatedConnection_r1)

add_executable(${LF_MAIN_TARGET})
include($ENV{REACTOR_UC_PATH}/cmake/lfc.cmake)
lf_setup()
lf_run_lfc(${PROJECT_ROOT}/src ${LF_MAIN})
lf_build_generated_code(${LF_MAIN_TARGET} ${CMAKE_CURRENT_SOURCE_DIR}/src-gen/${LF_MAIN}/${FEDERATE})
