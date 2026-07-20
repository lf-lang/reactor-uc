# Top-level test orchestrator for reactor-uc.
# Reached from the root CMakeLists when BUILD_UNIT_TESTS or BUILD_LF_TESTS is on.
# Prerequisites: reactor-uc target defined by the root CMakeLists, and
# include(CTest) called at root scope so the top-level CTestTestfile.cmake
# exists (otherwise `ctest` from the build dir finds no tests).

# Declares the TEST_COVERAGE option, defines lf_register_for_coverage and
# lf_create_coverage_target, and (when TEST_COVERAGE=ON) applies coverage
# flags and instruments reactor-uc. Must run before subdirectories register
# test executables so the helper function is in scope at their call sites.
include(${CMAKE_CURRENT_LIST_DIR}/coverage.cmake)

if(BUILD_UNIT_TESTS)
  add_subdirectory(unit)
endif()

add_test(
  NAME logging_format_check
  COMMAND ${CMAKE_COMMAND}
          -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
          -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
          -DBINARY_DIR=${CMAKE_BINARY_DIR}
          -P ${CMAKE_CURRENT_LIST_DIR}/compile/logging_format_check.cmake
)

if(BUILD_LF_TESTS)
  include(${CMAKE_SOURCE_DIR}/cmake/ulfc.cmake)
  include(${CMAKE_CURRENT_LIST_DIR}/lf/lf_tests.cmake)
endif()

# Create a coverage target that depends on all test executables registered with lf_register_for_coverage. 
lf_create_coverage_target()
