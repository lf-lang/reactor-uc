if(NOT DEFINED CMAKE_C_COMPILER OR NOT DEFINED SOURCE_DIR OR NOT DEFINED BINARY_DIR)
  message(FATAL_ERROR "CMAKE_C_COMPILER, SOURCE_DIR, and BINARY_DIR are required")
endif()

set(VALID_OBJECT_FILE "${BINARY_DIR}/logging_format_valid.o")
execute_process(
  COMMAND "${CMAKE_C_COMPILER}"
          -Wformat
          -Werror
          -I "${SOURCE_DIR}/include"
          -c "${SOURCE_DIR}/test/compile/logging_format_valid.c"
          -o "${VALID_OBJECT_FILE}"
  RESULT_VARIABLE VALID_COMPILE_RESULT
  OUTPUT_VARIABLE VALID_COMPILE_STDOUT
  ERROR_VARIABLE VALID_COMPILE_STDERR
)
file(REMOVE "${VALID_OBJECT_FILE}")

if(NOT VALID_COMPILE_RESULT EQUAL 0)
  message(FATAL_ERROR
          "Valid log format call failed to compile:\n${VALID_COMPILE_STDOUT}\n${VALID_COMPILE_STDERR}")
endif()

set(INVALID_OBJECT_FILE "${BINARY_DIR}/logging_format_mismatch.o")
execute_process(
  COMMAND "${CMAKE_C_COMPILER}"
          -Wformat
          -Werror
          -I "${SOURCE_DIR}/include"
          -c "${SOURCE_DIR}/test/compile/logging_format_mismatch.c"
          -o "${INVALID_OBJECT_FILE}"
  RESULT_VARIABLE INVALID_COMPILE_RESULT
  OUTPUT_VARIABLE INVALID_COMPILE_STDOUT
  ERROR_VARIABLE INVALID_COMPILE_STDERR
)
file(REMOVE "${INVALID_OBJECT_FILE}")

if(INVALID_COMPILE_RESULT EQUAL 0)
  message(FATAL_ERROR
          "Compiler accepted an invalid log format call; format checking is inactive")
endif()
