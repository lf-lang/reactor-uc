# Reusable LF test function definitions for reactor-uc.
# Prerequisites before including this file:
#   - REACTOR_UC_PATH is set
#   - reactor-uc target exists
#   - CTest is included
#   - lf_register_for_coverage(target) function is defined (real or stub)

set(LF_TEST_DIR ${CMAKE_CURRENT_LIST_DIR})
set(LF_TEST_TIMEOUT 30 CACHE STRING "Default timeout in seconds for LF tests")

# Build one executable from a generated Include.cmake directory.
# Optional extra arguments are added as additional include directories.
function(lf_add_executable TARGET_NAME SRC_GEN_DIR)
  if(NOT EXISTS ${SRC_GEN_DIR}/Include.cmake)
    message(WARNING "No Include.cmake in ${SRC_GEN_DIR}, skipping")
    return()
  endif()

  include(${SRC_GEN_DIR}/Include.cmake)

  add_executable(${TARGET_NAME} ${LFC_GEN_SOURCES} ${LFC_GEN_MAIN})
  target_link_libraries(${TARGET_NAME} PRIVATE reactor-uc)
  target_include_directories(${TARGET_NAME} PRIVATE ${LFC_GEN_INCLUDE_DIRS} ${ARGN})
  target_compile_definitions(${TARGET_NAME} PRIVATE ${LFC_GEN_COMPILE_DEFS})
  set_target_properties(${TARGET_NAME} PROPERTIES C_CLANG_TIDY "") # Disable clang-tidy on generated code
endfunction()

# Register a non-federated LF test target.
# Optional extra arguments are forwarded as additional include directories.
function(register_lf_test TEST_NAME SRC_GEN_DIR)
  lf_add_executable(${TEST_NAME} ${SRC_GEN_DIR} ${ARGN})
  if(NOT TARGET ${TEST_NAME})
    message(FATAL_ERROR "Failed to create target ${TEST_NAME} from ${SRC_GEN_DIR}")
  endif()

  add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
  set_tests_properties(${TEST_NAME} PROPERTIES TIMEOUT ${LF_TEST_TIMEOUT})
  lf_register_for_coverage(${TEST_NAME})
endfunction()

# Register a federated LF test target (multiple federates launched via script).
function(register_federated_lf_test TEST_NAME SRC_GEN_DIR)
  file(GLOB FEDERATE_CANDIDATES RELATIVE ${SRC_GEN_DIR} ${SRC_GEN_DIR}/*)
  set(FEDERATE_COMMAND_ARGS "")
  foreach(FEDERATE ${FEDERATE_CANDIDATES})
    if(NOT IS_DIRECTORY ${SRC_GEN_DIR}/${FEDERATE})
      continue()
    endif()
    set(FED_TARGET "${TEST_NAME}_r${FEDERATE}")
    lf_add_executable(${FED_TARGET} ${SRC_GEN_DIR}/${FEDERATE})
    if(NOT TARGET ${FED_TARGET})
      message(FATAL_ERROR "Failed to create federate target ${FED_TARGET} from ${SRC_GEN_DIR}/${FEDERATE}")
    endif()
    list(APPEND FEDERATE_COMMAND_ARGS "$<TARGET_FILE:${FED_TARGET}>")
    lf_register_for_coverage(${FED_TARGET})
  endforeach()

  add_test(
    NAME ${TEST_NAME}
    COMMAND bash ${LF_TEST_DIR}/run_federated_test.sh ${FEDERATE_COMMAND_ARGS}
  )
  set_tests_properties(${TEST_NAME} PROPERTIES
    TIMEOUT ${LF_TEST_TIMEOUT}
    RESOURCE_LOCK "federated_network"
  )
endfunction()
