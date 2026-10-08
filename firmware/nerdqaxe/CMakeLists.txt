# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
#cmake_minimum_required(VERSION 3.5)
cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)

add_definitions(-DLV_LVGL_H_INCLUDE_SIMPLE)

# Check if the environment variable BOARD is defined
if(DEFINED ENV{BOARD})
    add_compile_definitions($ENV{BOARD})
else()
    message(FATAL_ERROR "Environment variable BOARD is not set")
endif()

message(STATUS "Selected board: $ENV{BOARD}")

# Public helpers bind the application descriptor to a separate exact model family.
# An inherited board definition alone is not a public build or hardware qualification.
if(DEFINED FIVETRATUM_RELEASE_PROFILE)
    if(NOT "$ENV{BOARD}" STREQUAL "${FIVETRATUM_RELEASE_PROFILE}")
        message(FATAL_ERROR "Public release profile does not match BOARD")
    endif()
    if(FIVETRATUM_RELEASE_PROFILE STREQUAL "NERDQAXEPLUS2")
        set(_fivetratum_version_file "version.txt")
        set(_fivetratum_family "qa")
    elseif(FIVETRATUM_RELEASE_PROFILE STREQUAL "NERDOCTAXEGAMMA")
        set(_fivetratum_version_file "version-oct.txt")
        set(_fivetratum_family "oct")
    else()
        message(FATAL_ERROR "Unknown public Nerd release profile")
    endif()
    file(READ "${CMAKE_CURRENT_LIST_DIR}/${_fivetratum_version_file}" _fivetratum_version)
    string(STRIP "${_fivetratum_version}" _fivetratum_version)
    string(LENGTH "${_fivetratum_version}" _fivetratum_version_length)
    if(NOT _fivetratum_version MATCHES "^5tratumFW-${_fivetratum_family}-[0-9]+\\.[0-9]+\\.[0-9]+-beta\\.[0-9]+$"
       OR _fivetratum_version_length GREATER 31)
        message(FATAL_ERROR "Invalid model-specific public BETA version")
    endif()
    if(DEFINED PROJECT_VER AND NOT PROJECT_VER STREQUAL _fivetratum_version)
        message(FATAL_ERROR "PROJECT_VER does not match the selected model version file")
    endif()
    set(PROJECT_VER "${_fivetratum_version}")
endif()

set(ENV{SOURCE_DATE_EPOCH} 0)


add_compile_definitions(LV_MEM_CUSTOM=1)
add_compile_definitions(LV_MEM_CUSTOM_INCLUDE=<lvgl_psram.h>)
add_compile_definitions(LV_MEM_CUSTOM_ALLOC=lv_psram_alloc)
add_compile_definitions(LV_MEM_CUSTOM_FREE=lv_psram_free)
add_compile_definitions(LV_MEM_CUSTOM_REALLOC=lv_psram_realloc)

#add_compile_definitions(CONFIG_FREERTOS_USE_TRACE_FACILITY=1)
#add_compile_definitions(CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS=1)

# adds -I./main; needed for components to have
# access to main include files
include_directories("./main")

# this are only header files
include_directories("./components/arduinojson")

# (Not part of the boilerplate)
# This example uses an extra component for common functions such as Wi-Fi and Ethernet connection.
# set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/common_components/protocol_examples_common)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)


project(esp-miner)
