# Coverage configuration for reactor-uc tests. Included only from
# test/CMakeLists.txt (in-tree build). Coverage is not supported from the
# standalone test/lf build.

option(TEST_COVERAGE "Compute test coverage" OFF)

if(TEST_COVERAGE)
  set(CMAKE_BUILD_TYPE Debug)
  include(${CMAKE_SOURCE_DIR}/external/cmake/CodeCoverage.cmake)

  # Instrument only the reactor-uc library, not the tests themselves.
  # The link flag is INTERFACE so test executables linking reactor-uc.a pull
  # in the gcov runtime without being themselves instrumented at compile time.
  target_compile_options(reactor-uc PRIVATE --coverage -fprofile-update=atomic)
  target_link_options(reactor-uc INTERFACE --coverage)
endif()

# Record a target as a coverage dependency. No-op when coverage is off, so
# callers never need to gate on TEST_COVERAGE.
function(lf_register_for_coverage target)
  if(TEST_COVERAGE)
    set_property(GLOBAL APPEND PROPERTY LF_COVERAGE_DEPS ${target})
  endif()
endfunction()

# Create a coverage target that depends on all test executables registered with lf_register_for_coverage. Materialises the `coverage` lcov target when TEST_COVERAGE=ON, no-op otherwise.
function(lf_create_coverage_target)
  if(NOT TEST_COVERAGE)
    return()
  endif()
  get_property(_deps GLOBAL PROPERTY LF_COVERAGE_DEPS)
  # lcov 2.x treats unused patterns as a fatal 
  # `--ignore-errors unused` suppresses these errors
  setup_target_for_coverage_lcov(
    NAME coverage
    EXECUTABLE ctest
    EXCLUDE
      "external/**"
      "test/**"
      "src-gen/**"
    LCOV_ARGS    --rc lcov_branch_coverage=1 --ignore-errors unused
    GENHTML_ARGS --rc lcov_branch_coverage=1
    DEPENDENCIES ${_deps})
endfunction()
