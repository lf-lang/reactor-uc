# Reusable LF test definitions for reactor-uc.
# Prerequisites before including this file:
#   - REACTOR_UC_PATH is set
#   - reactor-uc target exists
#   - lfc.cmake is included (provides lf_run_lfc)
#   - CTest is included

set(LF_TEST_DIR ${CMAKE_CURRENT_LIST_DIR})
set(LF_TEST_BUILD_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# Build one executable from a generated Include.cmake directory.
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
  set_target_properties(${TARGET_NAME} PROPERTIES C_CLANG_TIDY "") # Disable clang-tidy on generated code
endfunction()

# Add a non-federated LF test target.
function(add_lf_test TEST_NAME SRC_GEN_DIR)
  add_lf_generated_target(${TEST_NAME} ${SRC_GEN_DIR})

  add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
  set_tests_properties(${TEST_NAME} PROPERTIES TIMEOUT 30)
  list(APPEND COVERAGE_DEPS ${TEST_NAME})
  set(COVERAGE_DEPS ${COVERAGE_DEPS} PARENT_SCOPE)
endfunction()

# Add a federated LF test target (multiple federates launched via script).
function(add_federated_lf_test TEST_NAME SRC_GEN_DIR)
  file(GLOB FEDERATE_CANDIDATES RELATIVE ${SRC_GEN_DIR} ${SRC_GEN_DIR}/*)
  set(FEDERATE_COMMAND_ARGS "")
  foreach(FEDERATE ${FEDERATE_CANDIDATES})
    if(IS_DIRECTORY ${SRC_GEN_DIR}/${FEDERATE} AND EXISTS ${SRC_GEN_DIR}/${FEDERATE}/Include.cmake)
      set(FED_TARGET "${TEST_NAME}_r${FEDERATE}")
      add_lf_generated_target(${FED_TARGET} ${SRC_GEN_DIR}/${FEDERATE})
      if(TARGET ${FED_TARGET})
        list(APPEND FEDERATE_COMMAND_ARGS "$<TARGET_FILE:${FED_TARGET}>")
        list(APPEND COVERAGE_DEPS ${FED_TARGET})
      endif()
    endif()
  endforeach()

  add_test(
    NAME ${TEST_NAME}
    COMMAND bash ${LF_TEST_DIR}/run_federated_test.sh ${FEDERATE_COMMAND_ARGS}
  )
  set_tests_properties(${TEST_NAME} PROPERTIES TIMEOUT 30)
  set(COVERAGE_DEPS ${COVERAGE_DEPS} PARENT_SCOPE)
endfunction()

# Discover and register all LF tests from a source directory.
# Options:
#   ONLY_BUILD  - build but don't register as CTest tests
#   FEDERATED   - treat as federated tests (multiple binaries per test)
function(add_lf_tests SRC_DIR SRC_GEN_BASE)
  cmake_parse_arguments(ARG "ONLY_BUILD;FEDERATED" "" "" ${ARGN})
  file(GLOB LF_FILES RELATIVE ${SRC_DIR} ${SRC_DIR}/*.lf)
  foreach(LF_FILE ${LF_FILES})
    string(REGEX REPLACE "\\.lf$" "" TEST_NAME ${LF_FILE})
    lf_run_lfc(${SRC_DIR} ${TEST_NAME})
    if(NOT ARG_ONLY_BUILD)
      set(SRC_GEN_DIR ${SRC_GEN_BASE}/${TEST_NAME})
      if(ARG_FEDERATED)
        add_federated_lf_test(${TEST_NAME} ${SRC_GEN_DIR} "${ARG_ONLY_BUILD}")
      else()
        add_lf_test(${TEST_NAME} ${SRC_GEN_DIR} "${ARG_ONLY_BUILD}")
      endif()
    endif()
  endforeach()
  set(COVERAGE_DEPS ${COVERAGE_DEPS} PARENT_SCOPE)
endfunction()

# --- Test discovery ---

# Main tests (build + run)
add_lf_tests(${LF_TEST_DIR}/src ${LF_TEST_BUILD_DIR}/src-gen)

# Legacy tests (build + run)
add_lf_tests(${LF_TEST_DIR}/src/legacy ${LF_TEST_BUILD_DIR}/src-gen/legacy)

# Federated tests (build + run)
add_lf_tests(${LF_TEST_DIR}/src/federated ${LF_TEST_BUILD_DIR}/src-gen/federated FEDERATED)

# Only-build tests (build, no run)
add_lf_tests(${LF_TEST_DIR}/src/only_build ${LF_TEST_BUILD_DIR}/src-gen/only_build FEDERATED ONLY_BUILD)
