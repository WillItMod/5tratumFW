cmake_minimum_required(VERSION 3.16)
project(five_tratum_candidate_host C CXX)
get_filename_component(FW_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${FW_ROOT}/main/displays/ui.cpp"
    "${FW_ROOT}/main/displays/displayDriver.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/extract_candidate_methods.py")
execute_process(COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_LIST_DIR}/extract_candidate_methods.py" "${FW_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}"
    RESULT_VARIABLE EXTRACT_RESULT)
if(NOT EXTRACT_RESULT EQUAL 0)
    message(FATAL_ERROR "Could not extract production candidate methods")
endif()
set(LV_CONF_PATH "${FW_ROOT}/main/displays/lv_conf.h" CACHE STRING "" FORCE)
add_subdirectory("${FW_ROOT}/managed_components/lvgl__lvgl" lvgl EXCLUDE_FROM_ALL)
target_include_directories(lvgl PUBLIC "${FW_ROOT}/test/host/display_native")
add_executable(candidate-native candidate_native.cpp
    "${FW_ROOT}/main/displays/images/ui_font_OpenSansBold13.c"
    "${FW_ROOT}/main/displays/images/ui_font_OpenSansBold14.c"
    "${FW_ROOT}/main/displays/images/ui_font_OpenSansBold24.c"
    "${FW_ROOT}/main/displays/images/ui_font_OpenSansBold45.c")
target_compile_features(candidate-native PRIVATE cxx_std_17)
target_compile_options(candidate-native PRIVATE -UNDEBUG -Wall -Wextra -Werror)
target_include_directories(candidate-native PRIVATE "${FW_ROOT}/main" "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(candidate-native PRIVATE lvgl m)
