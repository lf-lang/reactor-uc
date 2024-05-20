set(TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(TEST_SUFFIX test.c)  # Files that are tests must have names ending with TEST_SUFFIX.
set(TEST_MOCK_SRCS ${TEST_DIR}/mock.c)

# Find all the tests in the current dir
file(
  GLOB_RECURSE TEST_SOURCES
  LIST_DIRECTORIES false
  RELATIVE ${TEST_DIR}
  *${TEST_SUFFIX}
)

# Create executables for each test.
foreach(FILE ${TEST_SOURCES})
    string(REGEX REPLACE "[./]" "_" NAME ${FILE})
    add_executable(${NAME} ${TEST_DIR}/${FILE} ${TEST_MOCK_SRCS})
    add_test(NAME ${NAME} COMMAND ${NAME})
    target_link_libraries(
        ${NAME} PRIVATE
        reactor-uc Unity m
    )
  set_target_properties(${NAME} PROPERTIES C_CLANG_TIDY "") # Disable clang-tidy for this external lib.

  if(TEST_COVERAGE)
    set(COVERAGE_TARGET ${NAME}_cov)
    setup_target_for_coverage_lcov(
      NAME ${COVERAGE_TARGET}
      EXECUTABLE ${NAME}
      EXCLUDE 
        "external/Unity/**"
        "examples/"
      LCOV_ARGS --rc lcov_branch_coverage=1
      GENHTML_ARGS --rc lcov_branch_coverage=1)

    add_custom_target(run_lcov ALL
      DEPENDS ${COVERAGE_TARGET}
    )
    add_custom_command(
      TARGET run_lcov
      POST_BUILD
      COMMAND ${CMAKE_MAKE_PROGRAM} -C ${CMAKE_BINARY_DIR} ${COVERAGE_TARGET}
      COMMENT "Running lcov on the test"
    )
  endif()

endforeach(FILE ${TEST_FILES})

