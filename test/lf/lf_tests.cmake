# LF test compilation and registration for reactor-uc.
# Prerequisites before including this file:
#   - REACTOR_UC_PATH is set
#   - reactor-uc target exists
#   - lfc.cmake is included (provides lf_run_lfc)
#   - lf_register_for_coverage(target) function is defined (real or stub)
#   - CTest is included

include(${CMAKE_CURRENT_LIST_DIR}/lf_test_functions.cmake)

# LF_SKIP_GENERATE is a one-shot flag: pass -DLF_SKIP_GENERATE=ON each cmake
# invocation to skip LFC. Intentionally not declared via option(). We read
# and then unset the cache entry so the value never sticks across invocations.
# Forgetting to re-pass the flag falls back to full regeneration (safe default).
set(_LF_SKIP_GENERATE "${LF_SKIP_GENERATE}")
unset(LF_SKIP_GENERATE CACHE)

set(LF_TEST_BUILD_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# Reconfigure when .lf files are added or removed from test directories.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  ${LF_TEST_DIR}/src
  ${LF_TEST_DIR}/src/legacy
  ${LF_TEST_DIR}/src/federated
  ${LF_TEST_DIR}/src/only_build
)

# Non-federated files can be batch-compiled in a single LFC invocation
file(GLOB _MAIN_LF_FILES    ${LF_TEST_DIR}/src/*.ulf)
file(GLOB _LEGACY_LF_FILES  ${LF_TEST_DIR}/src/legacy/*.ulf)

set(_BATCH_LF_FILES ${_MAIN_LF_FILES} ${_LEGACY_LF_FILES})
# Track content edits even when skipping regeneration.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${_BATCH_LF_FILES})
if(NOT _LF_SKIP_GENERATE)
  lf_run_lfc(OUTPUT_DIR ${LF_TEST_BUILD_DIR} FILES ${_BATCH_LF_FILES})
endif()

# Federated files must be compiled individually
file(GLOB _FED_LF_FILES     ${LF_TEST_DIR}/src/federated/*.ulf)
file(GLOB _BUILD_LF_FILES   ${LF_TEST_DIR}/src/only_build/*.ulf)
foreach(_LF_FILE ${_FED_LF_FILES} ${_BUILD_LF_FILES})
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${_LF_FILE})
  if(NOT _LF_SKIP_GENERATE)
    lf_run_lfc(OUTPUT_DIR ${LF_TEST_BUILD_DIR} FILES ${_LF_FILE})
  endif()
endforeach()

# --- Test registration (targets + CTest) ---

# Main tests
foreach(_LF_FILE ${_MAIN_LF_FILES})
  get_filename_component(_TEST_NAME ${_LF_FILE} NAME_WE)
  register_lf_test(${_TEST_NAME} ${LF_TEST_BUILD_DIR}/src-gen/${_TEST_NAME})
endforeach()

# Legacy tests (add LF source dir as include path for auxiliary files like hello.h)
foreach(_LF_FILE ${_LEGACY_LF_FILES})
  get_filename_component(_TEST_NAME ${_LF_FILE} NAME_WE)
  get_filename_component(_LF_SRC_DIR ${_LF_FILE} DIRECTORY)
  register_lf_test(${_TEST_NAME} ${LF_TEST_BUILD_DIR}/src-gen/legacy/${_TEST_NAME} ${_LF_SRC_DIR})
endforeach()

# Federated tests
foreach(_LF_FILE ${_FED_LF_FILES})
  get_filename_component(_TEST_NAME ${_LF_FILE} NAME_WE)
  register_federated_lf_test(${_TEST_NAME} ${LF_TEST_BUILD_DIR}/src-gen/federated/${_TEST_NAME})
endforeach()
