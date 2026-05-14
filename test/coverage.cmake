# Coverage configuration for reactor-uc tests. Included only from
# test/CMakeLists.txt (in-tree build). Coverage is not supported from the
# standalone test/lf build.

option(TEST_COVERAGE "Compute test coverage" OFF)

if(TEST_COVERAGE)
  set(CMAKE_BUILD_TYPE Debug)
  include(${CMAKE_SOURCE_DIR}/external/cmake/CodeCoverage.cmake)
  append_coverage_compiler_flags()            # directory scope: reaches test exes
  add_compile_options(-fprofile-update=atomic)

  # Instrument the main library target so that coverage is collected for it when running tests.
  target_compile_options(reactor-uc PRIVATE --coverage -fprofile-update=atomic)
  target_link_options(reactor-uc PRIVATE --coverage)
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
  setup_target_for_coverage_lcov(
    NAME coverage
    EXECUTABLE ctest
    EXCLUDE
      "external/**"
      "test/**"
      "src-gen/**"
    LCOV_ARGS    --rc lcov_branch_coverage=1
    GENHTML_ARGS --rc lcov_branch_coverage=1
    DEPENDENCIES ${_deps})
endfunction()
