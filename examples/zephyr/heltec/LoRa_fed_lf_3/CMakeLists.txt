cmake_minimum_required(VERSION 3.20.0)

# 1. Locate Zephyr environment
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(LoRa_fed_lf_3)

# 2. Set target platform for reactor-uc
set(PLATFORM "ZEPHYR" CACHE STRING "Set platform to Zephyr" FORCE)

# 2.5 Set runtime path if not already defined
if(NOT DEFINED RUNTIME_PATH)
    # Get the reactor-uc root path from REACTOR_UC_PATH env var or compute it
    if(DEFINED ENV{REACTOR_UC_PATH})
        set(RUNTIME_PATH "$ENV{REACTOR_UC_PATH}" CACHE STRING "Path to reactor-uc runtime")
    else()
        # Default to relative path: examples/zephyr/heltec/LoRa_fed_lf_3 -> ../../..
        get_filename_component(RUNTIME_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
    endif()
    
    if(NOT EXISTS "${RUNTIME_PATH}/CMakeLists.txt")
        message(FATAL_ERROR "Cannot find reactor-uc runtime at ${RUNTIME_PATH}. Please set REACTOR_UC_PATH environment variable.")
    endif()
endif()

# 3. Load generated build configuration from lfc
# For federated reactors, select the appropriate federate (tx, rx, etc.)
set(FEDERATE "tx" CACHE STRING "Select federate for federated reactor")
set(LF_GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src-gen/LoRa_fed_lf_3/${FEDERATE}")
if(EXISTS "${LF_GEN_DIR}/Include.cmake")
    include("${LF_GEN_DIR}/Include.cmake")
else()
    message(FATAL_ERROR "src-gen/LoRa_fed_lf_3/${FEDERATE}/Include.cmake not found. Run lfc/ulfc first.")
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

target_compile_definitions(app PRIVATE
    ${LFC_GEN_COMPILE_DEFS}
    PLATFORM_ZEPHYR
    LF_LOG_LEVEL_ALL=LF_LOG_LEVEL_INFO
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
    target_compile_definitions(reactor-uc PUBLIC
        ${LFC_GEN_COMPILE_DEFS}
        LF_LOG_LEVEL_ALL=LF_LOG_LEVEL_INFO
    )
    target_link_libraries(app PRIVATE reactor-uc)
endif()