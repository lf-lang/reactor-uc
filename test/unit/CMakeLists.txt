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
    add_custom_command(TARGET
        ${NAME} POST_BUILD
        COMMAND "${CMAKE_PREFIX_PATH}size" "${NAME}" "|" "tee" "${NAME}.size"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
  set_target_properties(${NAME} PROPERTIES C_CLANG_TIDY "") # Disable clang-tidy for this external lib.
endforeach(FILE ${TEST_FILES})

if(TEST_COVERAGE)
  setup_target_for_coverage_lcov(
    NAME coverage
    EXECUTABLE ctest
      EXCLUDE 
        "external/Unity/**"
        "examples/**"
        "test/**"
      LCOV_ARGS --rc lcov_branch_coverage=1
      GENHTML_ARGS --rc lcov_branch_coverage=1
      DEPENDENCIES reaction_queue_test_c event_queue_test_c)
  endif()

