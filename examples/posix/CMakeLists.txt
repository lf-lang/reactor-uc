cmake_minimum_required(VERSION 3.20.0)
set(PLATFORM "POSIX" CACHE STRING "Platform to target")
set(NETWORK_POSIX_TCP ON)

# Find all .c files in the src directory
file(GLOB C_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.c")

# Create a list to store all executable names
set(EXECUTABLES)

# Loop through each .c file and create an executable
foreach(C_FILE ${C_FILES})
    # Get the filename without the extension
    get_filename_component(EXEC_NAME ${C_FILE} NAME_WE)
    
    # Add the executable
    add_executable(${EXEC_NAME} ${C_FILE})
    
    # Link the necessary libraries
    target_link_libraries(${EXEC_NAME} PUBLIC reactor-uc nanopb)
    
    # Add the executable name to the list
    list(APPEND EXECUTABLES ${EXEC_NAME})
endforeach()

# Define a custom target to run all executables
add_custom_target(examples
    COMMAND ${CMAKE_COMMAND} -E echo "Running all executables"
    DEPENDS ${EXECUTABLES}
)
