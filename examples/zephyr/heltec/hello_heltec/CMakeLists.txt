cmake_minimum_required(VERSION 3.20.0)

# 1. Locate Zephyr environment
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(hello_heltec)

# 2. Set target platform for reactor-uc
set(PLATFORM "ZEPHYR" CACHE STRING "Set platform to Zephyr" FORCE)

# 3. Load generated build configuration from lfc
set(LF_GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src-gen/hello_heltec")
if(EXISTS "${LF_GEN_DIR}/Include.cmake")
    include("${LF_GEN_DIR}/Include.cmake")
else()
    message(FATAL_ERROR "src-gen/hello_heltec/Include.cmake not found. Run lfc/ulfc first.")
endif()

# 4. Add reactor-uc runtime
if(DEFINED RUNTIME_PATH AND EXISTS "${RUNTIME_PATH}")
    add_subdirectory("${RUNTIME_PATH}" build_reactor_uc EXCLUDE_FROM_ALL)
endif()

# 5. Bind generated C sources to Zephyr's 'app' target
target_sources(app PRIVATE 
    ${LFC_GEN_MAIN} 
    ${LFC_GEN_SOURCES}
)

# 6. Add search paths for generated and core runtime headers
target_include_directories(app PRIVATE 
    ${LFC_GEN_INCLUDE_DIRS}
    ${LF_GEN_DIR}
    ${LF_GEN_DIR}/include
    ${LF_GEN_DIR}/core
    ${LF_GEN_DIR}/core/api
    ${LF_GEN_DIR}/low_level_platform/api
)

# 7. Link reactor-uc library
if(TARGET reactor-uc)
    target_link_libraries(app PRIVATE reactor-uc)
endif()