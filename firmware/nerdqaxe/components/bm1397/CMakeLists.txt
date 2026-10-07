idf_component_register(
SRCS
    "asic.cpp"
    "bm1366.cpp"
    "bm1368.cpp"
    "bm1370.cpp"
    "bm1370_protocol.cpp"
    "bm1373.cpp"
    "serial.cpp"
    "crc.cpp"
    "mining_utils.cpp"
    "mining.cpp"

INCLUDE_DIRS
    "include"

REQUIRES
    "freertos"
    "driver"
    "mbedtls"
    "tcp_transport"
    "esp_timer"
)

option(FIVETRATUM_ASIC_CAPTURE_LOGS "Log existing ASIC TX/RX frames for protocol investigation" OFF)
if(FIVETRATUM_ASIC_CAPTURE_LOGS)
    # Existing upstream logging only: no selector, extra UART command, register
    # write, frequency change or saved setting is introduced by this option.
    target_compile_definitions(${COMPONENT_LIB} PRIVATE ASIC_SERIALTX_DEBUG=1 ASIC_SERIALRX_DEBUG=1)
endif()

option(FIVETRATUM_BM1370_DIAGNOSTIC_DRIVER "Use captured-format BM1370 CRC decoder and dedicated chain job codec (unqualified diagnostics)" OFF)
# PUBLIC is essential: board translation units allocate BM1370, whose bounded
# decoder members depend on this flag. Every dependent target sees the same 0/1
# definition and therefore the same class layout. No selector is enabled.
target_compile_definitions(${COMPONENT_LIB} PUBLIC
    FIVETRATUM_BM1370_DIAGNOSTIC_DRIVER=$<BOOL:${FIVETRATUM_BM1370_DIAGNOSTIC_DRIVER}>)

option(FIVETRATUM_BM1370_CAPTURE "Enable RAM-only passive BM1370 diagnostic capture API" OFF)
if(FIVETRATUM_BM1370_CAPTURE AND NOT FIVETRATUM_BM1370_DIAGNOSTIC_DRIVER)
    message(FATAL_ERROR "BM1370 capture requires the diagnostic driver; no selector is enabled")
endif()
target_compile_definitions(${COMPONENT_LIB} PUBLIC
    FIVETRATUM_BM1370_CAPTURE=$<BOOL:${FIVETRATUM_BM1370_CAPTURE}>)
if(FIVETRATUM_BM1370_CAPTURE)
    target_sources(${COMPONENT_LIB} PRIVATE "bm1370_capture.cpp" "bm1370_capture_runtime.cpp")
endif()

# Include the header files from "main" directory
target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../main")

# Include the header files from "main/tasks" directory
target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../main/tasks")
