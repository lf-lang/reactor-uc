set(UNITY_DIR ${CMAKE_SOURCE_DIR}/external/Unity)
add_library(Unity STATIC ${UNITY_DIR}/src/unity.c)
target_include_directories(Unity PUBLIC ${UNITY_DIR}/src)
set_target_properties(Unity PROPERTIES C_CLANG_TIDY "") # Disable clang-tidy for this external lib.

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

if(NOT LF_RUNTIME_EXTENSIONS)
  list(FILTER TEST_SOURCES EXCLUDE REGEX "^(extension_|trigger_lifecycle)")
endif()

# Create executables for each test.
foreach(FILE ${TEST_SOURCES})
    string(REGEX REPLACE "[./]" "_" NAME ${FILE})
    add_executable(${NAME} ${TEST_DIR}/${FILE} ${TEST_MOCK_SRCS})
    target_compile_definitions(${NAME} PRIVATE "PLATFORM_${PLATFORM}")
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
  lf_register_for_coverage(${NAME})
endforeach(FILE ${TEST_FILES})

if(LF_RUNTIME_EXTENSIONS)
  # The registry's idempotence guard regresses as NONTERMINATION, not as a failed assertion:
  # a duplicate registration makes the list a cycle and Environment_notify_tag_final never
  # returns. Without a bound, such a regression stalls the suite for ctest's 1500s default.
  set_tests_properties(extension_registry_test_c PROPERTIES TIMEOUT 30)
endif()
