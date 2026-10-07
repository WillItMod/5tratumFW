cmake_minimum_required(VERSION 3.16)
project(five_tratum_display_host C CXX)
get_filename_component(FW_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(LV_CONF_PATH "${FW_ROOT}/main/displays/lv_conf.h" CACHE STRING "" FORCE)
add_subdirectory("${FW_ROOT}/managed_components/lvgl__lvgl" lvgl EXCLUDE_FROM_ALL)
# Only allocation is substituted for desktop rendering. Pixel depth, fonts,
# native layout and LVGL version are the actual firmware configuration.
target_include_directories(lvgl PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
add_executable(display-native display_native.cpp
    "${FW_ROOT}/main/displays/images/ui_font_OpenSansBold13.c"
    "${FW_ROOT}/main/displays/images/ui_font_OpenSansBold14.c"
    "${FW_ROOT}/main/displays/images/ui_font_OpenSansBold24.c"
    "${FW_ROOT}/main/displays/images/ui_font_OpenSansBold45.c")
target_compile_features(display-native PRIVATE cxx_std_17)
target_compile_options(display-native PRIVATE -UNDEBUG -Wall -Wextra -Werror)
target_include_directories(display-native PRIVATE "${FW_ROOT}/main")
target_link_libraries(display-native PRIVATE lvgl m)
