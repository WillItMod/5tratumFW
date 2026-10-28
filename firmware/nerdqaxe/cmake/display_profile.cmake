# Display selection is independent of miner hardware and flash geometry.
# This helper is also configured by the host test; no ESP-IDF or hardware is
# required to exercise the exact public/private build admission policy.
set(FIVETRATUM_DISPLAY_PROFILE "TDISPLAY_S3_320X170" CACHE STRING "Display-only profile")
set_property(CACHE FIVETRATUM_DISPLAY_PROFILE PROPERTY STRINGS
    TDISPLAY_S3_320X170 QAXE_480X320_RGB QAXE_480X320_BGR)
option(FIVETRATUM_EXPERIMENTAL_DISPLAY "Enable an unqualified, private QAxe display build" OFF)
# QAxe++ and OctAxe Gamma builds retain the original boot default and include
# explicit, persisted experimental choices. Other targets cannot select them.
# The separate fixed experimental default remains restricted to private QAxe.
if("$ENV{BOARD}" STREQUAL "NERDQAXEPLUS2" OR
   "$ENV{BOARD}" STREQUAL "NERDOCTAXEGAMMA")
    set(FIVETRATUM_RUNTIME_DISPLAY_SELECTOR ON CACHE BOOL "Model-qualified runtime LCD selection" FORCE)
    add_compile_definitions(FIVETRATUM_RUNTIME_WIDE_DISPLAY=1)
else()
    set(FIVETRATUM_RUNTIME_DISPLAY_SELECTOR OFF CACHE BOOL "Model-qualified runtime LCD selection" FORCE)
    add_compile_definitions(FIVETRATUM_RUNTIME_WIDE_DISPLAY=0)
endif()
if(FIVETRATUM_DISPLAY_PROFILE STREQUAL "TDISPLAY_S3_320X170")
    add_compile_definitions(FIVETRATUM_LCD_PROFILE=0)
elseif(FIVETRATUM_DISPLAY_PROFILE STREQUAL "QAXE_480X320_RGB" OR
       FIVETRATUM_DISPLAY_PROFILE STREQUAL "QAXE_480X320_BGR")
    if(NOT "$ENV{BOARD}" STREQUAL "NERDQAXEPLUS2" OR NOT FIVETRATUM_EXPERIMENTAL_DISPLAY)
        message(FATAL_ERROR "480x320 requires BOARD=NERDQAXEPLUS2 and explicit experimental opt-in")
    endif()
    if(DEFINED FIVETRATUM_RELEASE_PROFILE)
        message(FATAL_ERROR "Experimental displays are not admitted by the public OTA packaging/updater policy")
    endif()
    if(FIVETRATUM_DISPLAY_PROFILE STREQUAL "QAXE_480X320_RGB")
        add_compile_definitions(FIVETRATUM_LCD_PROFILE=1)
    else()
        add_compile_definitions(FIVETRATUM_LCD_PROFILE=2)
    endif()
else()
    message(FATAL_ERROR "Unknown 5tratumFW display profile")
endif()
message(STATUS "Selected display: ${FIVETRATUM_DISPLAY_PROFILE}")
