# LF test compilation and registration for reactor-uc.
# Prerequisites before including this file:
#   - REACTOR_UC_PATH is set
#   - reactor-uc target exists
#   - lfc.cmake is included (provides lf_run_lfc, lf_run_lfc_batch)
#   - LF_COVERAGE_DEPS global property is defined
#   - CTest is included

include(${CMAKE_CURRENT_LIST_DIR}/lf_test_functions.cmake)

set(LF_TEST_BUILD_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# Reconfigure when .lf files are added or removed from test directories.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  ${LF_TEST_DIR}/src
  ${LF_TEST_DIR}/src/legacy
  ${LF_TEST_DIR}/src/federated
  ${LF_TEST_DIR}/src/only_build
)

# Non-federated files can be batch-compiled in a single LFC invocation
file(GLOB _MAIN_LF_FILES    ${LF_TEST_DIR}/src/*.lf)
file(GLOB _LEGACY_LF_FILES  ${LF_TEST_DIR}/src/legacy/*.lf)

set(_BATCH_LF_FILES ${_MAIN_LF_FILES} ${_LEGACY_LF_FILES})
lf_run_lfc_batch(${LF_TEST_BUILD_DIR} ${_BATCH_LF_FILES})

# Federated files must be compiled individually 
file(GLOB _FED_LF_FILES     ${LF_TEST_DIR}/src/federated/*.lf)
file(GLOB _BUILD_LF_FILES   ${LF_TEST_DIR}/src/only_build/*.lf)
foreach(_LF_FILE ${_FED_LF_FILES} ${_BUILD_LF_FILES})
  get_filename_component(_LF_DIR ${_LF_FILE} DIRECTORY)
  get_filename_component(_LF_NAME ${_LF_FILE} NAME_WE)
  lf_run_lfc(${_LF_DIR} ${_LF_NAME})
endforeach()

# --- Test registration (targets + CTest) ---

# Main tests
foreach(_LF_FILE ${_MAIN_LF_FILES})
  get_filename_component(_TEST_NAME ${_LF_FILE} NAME_WE)
  register_lf_test(${_TEST_NAME} ${LF_TEST_BUILD_DIR}/src-gen/${_TEST_NAME})
endforeach()

# Legacy tests
foreach(_LF_FILE ${_LEGACY_LF_FILES})
  get_filename_component(_TEST_NAME ${_LF_FILE} NAME_WE)
  register_lf_test(${_TEST_NAME} ${LF_TEST_BUILD_DIR}/src-gen/legacy/${_TEST_NAME})
endforeach()

# Federated tests
foreach(_LF_FILE ${_FED_LF_FILES})
  get_filename_component(_TEST_NAME ${_LF_FILE} NAME_WE)
  register_federated_lf_test(${_TEST_NAME} ${LF_TEST_BUILD_DIR}/src-gen/federated/${_TEST_NAME})
endforeach()