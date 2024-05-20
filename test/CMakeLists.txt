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
endforeach(FILE ${TEST_FILES})
