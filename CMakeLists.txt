# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# if(EXISTS "${CMAKE_SOURCE_DIR}/fix_compile_commands.py")
#     add_custom_target(
#         fix_clangd ALL
#         COMMAND ${CMAKE_COMMAND} -E echo "Running fix_compile_commands.py..."
#         COMMAND ${CMAKE_COMMAND} -E env python3 ${CMAKE_SOURCE_DIR}/fix_compile_commands.py
#         COMMENT "Cleaning compile_commands.json for clangd"
#         VERBATIM
#     )
# endif()

include_directories($ENV{IDF_PATH}/components/esp_system/include)
add_compile_options("-Wno-attributes")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ESPMaestro)
