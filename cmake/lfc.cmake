cmake_minimum_required(VERSION 3.20.0)

set(LFC_RUNTIME_SYMLINK OFF CACHE BOOL "Use a symlink to the runtime instead of copying it into src-gen")

# Sets up default values for LF_MAIN and LOG_LEVEL which
# can be overridden by the user, either in the main CMakeLists.txt or
# from the command line.
function (lf_setup)
  if (NOT DEFINED LF_MAIN)
    message(FATAL_ERROR "LF_MAIN must be defined to be the name of the main LF source file without the .lf extension")
  endif()

  if (NOT DEFINED LOG_LEVEL)
    set(LOG_LEVEL "LF_LOG_LEVEL_INFO" PARENT_SCOPE)
  endif()
endfunction()

# Run the LFC compiler on one or more LF source files in a single invocation.
# Batching avoids paying JVM startup cost per file when possible.
# Args (keyword form):
#   FILES <path>...     Absolute paths to one or more .lf files.  Required.
#   OUTPUT_DIR <path>   Root output directory for generated code.  Defaults
#                       to CMAKE_CURRENT_SOURCE_DIR when omitted.
function(lf_run_lfc)
  cmake_parse_arguments(LFC "" "OUTPUT_DIR" "FILES" ${ARGN})

  if(NOT LFC_FILES)
    message(FATAL_ERROR "lf_run_lfc: FILES is required")
  endif()
  if(NOT LFC_OUTPUT_DIR)
    set(LFC_OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
  endif()

  foreach(_F ${LFC_FILES})
    if(NOT EXISTS ${_F})
      message(FATAL_ERROR "LF source file does not exist: ${_F}")
    endif()
  endforeach()

  set(LFC_COMMAND $ENV{REACTOR_UC_PATH}/ulf/bin/ulfc-dev -n -o ${LFC_OUTPUT_DIR} ${LFC_FILES})
  if(LFC_RUNTIME_SYMLINK)
    list(APPEND LFC_COMMAND --runtime-symlink)
  endif()

  list(LENGTH LFC_FILES _num_files)
  message(STATUS "Running LFC on ${_num_files} file(s)")
  execute_process(
    COMMAND ${LFC_COMMAND}
    ECHO_OUTPUT_VARIABLE
    COMMAND_ERROR_IS_FATAL ANY
  )
  
  # Make CMake reconfigure whenever any of the input files change
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${LFC_FILES})
endfunction()

# Build the generated code from the LFC compiler. This function should be called after lf_run_lfc.
# Args:
#   TARGET: The CMake target to build the generated code for. This target must already be defined.
#   SOURCE_GEN_DIR: The directory containing the generated code. This is typically src-gen/${LF_MAIN}. 
function(lf_build_generated_code MAIN_TARGET SOURCE_GEN_DIR)

  # Check that TARGET is defined
  if (NOT TARGET ${MAIN_TARGET})
    message(FATAL_ERROR "TARGET ${MAIN_TARGET} is not defined")
  endif()

  # Check if the SOURCE_GEN_DIR exists
  if (NOT EXISTS ${SOURCE_GEN_DIR})
    message(FATAL_ERROR "src-gen directory does not exist: ${SOURCE_GEN_DIR}")
  endif()

  # Check if the Include.cmake file exists in SOURCE_GEN_DIR
  if (NOT EXISTS ${SOURCE_GEN_DIR}/Include.cmake)
    message(FATAL_ERROR "Include.cmake does not exist in src-gen directory: ${SOURCE_GEN_DIR}/Include.cmake")
  endif()

  include(${SOURCE_GEN_DIR}/Include.cmake)
  add_subdirectory(${RUNTIME_PATH})
  target_sources(${MAIN_TARGET} PRIVATE ${LFC_GEN_MAIN} ${LFC_GEN_SOURCES})
  target_include_directories(${MAIN_TARGET} PRIVATE ${LFC_GEN_INCLUDE_DIRS})
  target_link_libraries(${MAIN_TARGET} PUBLIC reactor-uc)
  target_compile_definitions(reactor-uc PUBLIC LF_LOG_LEVEL_ALL=${LOG_LEVEL})
  target_compile_definitions(reactor-uc PUBLIC ${LFC_GEN_COMPILE_DEFS})
endfunction()