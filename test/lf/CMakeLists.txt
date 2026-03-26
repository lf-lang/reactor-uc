cmake_minimum_required(VERSION 3.10)
project(LFTests LANGUAGES C)

# Reactor-UC path: either from env or passed via -D
if(NOT DEFINED REACTOR_UC_PATH)
  set(REACTOR_UC_PATH $ENV{REACTOR_UC_PATH})
endif()

if(NOT REACTOR_UC_PATH)
  message(FATAL_ERROR "REACTOR_UC_PATH must be set (env var or -DREACTOR_UC_PATH=...)")
endif()

include(CTest)
set(LFC_RUNTIME_SYMLINK ON CACHE BOOL "Use a symlink to the runtime")
include(${REACTOR_UC_PATH}/cmake/lfc.cmake)

set(PLATFORM "POSIX" CACHE STRING "Target platform")
set(TEST_COVERAGE OFF CACHE BOOL "Compute test coverage")

# Code coverage setup
if(TEST_COVERAGE)
  set(CMAKE_BUILD_TYPE "Debug")
  include(${REACTOR_UC_PATH}/external/cmake/CodeCoverage.cmake)
  append_coverage_compiler_flags()
endif()

# Build runtime ONCE
add_compile_definitions(FEDERATED NETWORK_CHANNEL_TCP_POSIX)
add_subdirectory(${REACTOR_UC_PATH} reactor-uc-build)

# Set a default log level for all tests
target_compile_definitions(reactor-uc PUBLIC LF_LOG_LEVEL_ALL=LF_LOG_LEVEL_WARN)

# Common helper to build one executable from a generated Include.cmake file.
# Args:
#   TARGET_NAME: The executable target to create.
#   SRC_GEN_DIR: The generated directory containing Include.cmake.
function(add_lf_generated_target TARGET_NAME SRC_GEN_DIR)
  if(NOT EXISTS ${SRC_GEN_DIR}/Include.cmake)
    message(WARNING "No Include.cmake in ${SRC_GEN_DIR}, skipping")
    return()
  endif()

  include(${SRC_GEN_DIR}/Include.cmake)

  add_executable(${TARGET_NAME} ${LFC_GEN_SOURCES} ${LFC_GEN_MAIN})
  target_link_libraries(${TARGET_NAME} PRIVATE reactor-uc)
  target_include_directories(${TARGET_NAME} PRIVATE ${LFC_GEN_INCLUDE_DIRS})
  target_compile_definitions(${TARGET_NAME} PRIVATE ${LFC_GEN_COMPILE_DEFS})
endfunction()

# Helper function to add a non-federated LF test target from a src-gen directory.
# Args:
#   TEST_NAME: The name of the test target to create.
#   SRC_GEN_DIR: The directory containing the generated code for the test. This is typically src-gen/${TEST_NAME}.
#   ONLY_BUILD: If true, only build the test but do not add it as a test target since it is only meant to be built, not run. Default is false.
function(add_lf_test TEST_NAME SRC_GEN_DIR ONLY_BUILD)
  add_lf_generated_target(${TEST_NAME} ${SRC_GEN_DIR})

  if(NOT ONLY_BUILD)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    set_tests_properties(${TEST_NAME} PROPERTIES TIMEOUT 30)
  endif()

  install(TARGETS ${TEST_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    OPTIONAL
  )
endfunction()

# Helper function to add a federated LF test target.
# Args:
#   TEST_NAME: The name of the logical test.
#   SRC_GEN_DIR: The directory containing generated federate subdirectories.
#   ONLY_BUILD: If true, only build the federates and launcher target, but do not add a test.
function(add_federated_lf_test TEST_NAME SRC_GEN_DIR ONLY_BUILD)
  file(GLOB FEDERATE_CANDIDATES RELATIVE ${SRC_GEN_DIR} ${SRC_GEN_DIR}/*)
  set(FEDERATES "")
  foreach(FEDERATE ${FEDERATE_CANDIDATES})
    if(IS_DIRECTORY ${SRC_GEN_DIR}/${FEDERATE} AND EXISTS ${SRC_GEN_DIR}/${FEDERATE}/Include.cmake)
      list(APPEND FEDERATES ${FEDERATE})
    endif()
  endforeach()

  set(FEDERATE_COMMAND_ARGS "")
  set(FEDERATE_TARGETS "")
  foreach(FEDERATE ${FEDERATES})
    set(FED_TARGET "${TEST_NAME}_r${FEDERATE}")
    add_lf_generated_target(${FED_TARGET} ${SRC_GEN_DIR}/${FEDERATE})
    if(TARGET ${FED_TARGET})
      list(APPEND FEDERATE_TARGETS ${FED_TARGET})
      list(APPEND FEDERATE_COMMAND_ARGS "$<TARGET_FILE:${FED_TARGET}>")
    endif()
  endforeach()

  if(NOT ONLY_BUILD)
    add_test(
      NAME ${TEST_NAME}
      COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/run_federated_test.sh ${FEDERATE_COMMAND_ARGS}
    )
    set_tests_properties(${TEST_NAME} PROPERTIES TIMEOUT 30)
  endif()
endfunction()

# Fils all the tests in the src and legacy folder ending with .lf, run the LFC compiler on them, and add them as test targets.
file(GLOB LF_TESTS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/src/*.lf)
file(GLOB LF_LEGACY_TESTS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/legacy ${CMAKE_CURRENT_SOURCE_DIR}/legacy/*.lf)
file(GLOB LF_FEDERATED_TESTS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/src/federated ${CMAKE_CURRENT_SOURCE_DIR}/src/federated/*.lf)
list(APPEND LF_TESTS ${LF_LEGACY_TESTS})


foreach(LF_TEST ${LF_TESTS})
    # Get the test name by removing the .lf extension
    string(REGEX REPLACE "\\.lf$" "" TEST_NAME ${LF_TEST})
    set(SRC_GEN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src-gen/${TEST_NAME})
    lf_run_lfc(${CMAKE_CURRENT_SOURCE_DIR}/src ${TEST_NAME})
    add_lf_test(${TEST_NAME} ${SRC_GEN_DIR} FALSE)

endforeach(LF_TEST ${LF_TESTS})  

foreach(LF_TEST ${LF_FEDERATED_TESTS})
    string(REGEX REPLACE "\\.lf$" "" TEST_NAME ${LF_TEST})
    set(SRC_GEN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src-gen/federated/${TEST_NAME})
    lf_run_lfc(${CMAKE_CURRENT_SOURCE_DIR}/src/federated ${TEST_NAME})
    add_federated_lf_test(${TEST_NAME} ${SRC_GEN_DIR} FALSE)
endforeach(LF_TEST ${LF_FEDERATED_TESTS})

# Finds all the only_build tests in the only_build folder and runs the LFC compiler on them, but does not add them as test targets since they are only meant to be built, not run.
file(GLOB LF_ONLY_BUILD_TESTS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/only_build ${CMAKE_CURRENT_SOURCE_DIR}/only_build/*.lf)
foreach(LF_TEST ${LF_ONLY_BUILD_TESTS})
    # Get the test name by removing the .lf extension
    string(REGEX REPLACE "\\.lf$" "" TEST_NAME ${LF_TEST})
    set(SRC_GEN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src-gen/${TEST_NAME})
    lf_run_lfc(${CMAKE_CURRENT_SOURCE_DIR}/only_build ${TEST_NAME})
    add_lf_test(${TEST_NAME} ${SRC_GEN_DIR} TRUE)
    # Only build the test, do not add it as a test target since it is only meant to be built, not run.
endforeach(LF_TEST ${LF_ONLY_BUILD_TESTS})

# Coverage report target
if(TEST_COVERAGE)
  setup_target_for_coverage_lcov(
    BASE_DIRECTORY ${REACTOR_UC_PATH}
    NAME coverage
    EXECUTABLE ctest
    EXCLUDE "external/**" "test/**"
    LCOV_ARGS --rc lcov_branch_coverage=1
    GENHTML_ARGS --rc lcov_branch_coverage=1
  )
endif()
        
