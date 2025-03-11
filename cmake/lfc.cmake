cmake_minimum_required(VERSION 3.20.0)

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

# Run the LFC compiler on the specified LF source file, LF_SOURCE_DIR/LF_MAIN.lf. Also make the CMake configuration
# depend on any LF source file found within LF_SOURCE_DIR. This ensures that the LFC compiler is rerun whenever any LF
# source file changes.
# Args:
#   LF_SOURCE_DIR: The directory containing the LF source file.
#   LF_MAIN: The name of the LF source file without the .lf extension.
function(lf_run_lfc LF_SOURCE_DIR LF_MAIN)
    # Check if the LF_SOURCE_DIR exists
  if (NOT EXISTS ${LF_SOURCE_DIR})
    message(FATAL_ERROR "LF source directory does not exist: ${LF_SOURCE_DIR}")
  endif()

  # Check if the LF_MAIN file exists
  if (NOT EXISTS ${LF_SOURCE_DIR}/${LF_MAIN}.lf)
    message(FATAL_ERROR "LF main file does not exist: ${LF_SOURCE_DIR}/${LF_MAIN}.lf")
  endif()  

  set(LFC_COMMAND $ENV{REACTOR_UC_PATH}/lfc/bin/lfc-dev ${LF_SOURCE_DIR}/${LF_MAIN}.lf -n -o ${CMAKE_CURRENT_SOURCE_DIR})
  execute_process(COMMAND echo "Running LFC: ${LFC_COMMAND}")
  execute_process(
      COMMAND ${LFC_COMMAND}
      ECHO_OUTPUT_VARIABLE
      COMMAND_ERROR_IS_FATAL ANY

  )
  file(GLOB_RECURSE LF_SOURCES ${LF_SOURCE_DIR}/*.lf)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${LF_SOURCES})
  message(STATUS "Found LF sources: ${LF_SOURCES}")

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
  if (NOT EXISTS ${SOURCE_GEN_DIR}/${FEDERATE}/Include.cmake)
    message(FATAL_ERROR "Include.cmake does not exist in src-gen directory: ${SOURCE_GEN_DIR}/Include.cmake")
  endif()

  add_subdirectory(${REACTOR_UC_PATH})
  target_sources(${MAIN_TARGET} PRIVATE ${LFC_GEN_MAIN} ${LFC_GEN_SOURCES})
  target_include_directories(${MAIN_TARGET} PRIVATE ${LFC_GEN_INCLUDE_DIRS})
  target_link_libraries(${MAIN_TARGET} PUBLIC reactor-uc)
  target_compile_definitions(reactor-uc PUBLIC LF_LOG_LEVEL_ALL=${LOG_LEVEL})
  target_compile_definitions(reactor-uc PUBLIC LF_LOG_LEVEL_FED=LF_LOG_LEVEL_DEBUG)
  target_compile_definitions(reactor-uc PUBLIC ${LFC_GEN_COMPILE_DEFS})
endfunction()
