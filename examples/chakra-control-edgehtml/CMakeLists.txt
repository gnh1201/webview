cmake_minimum_required(VERSION 3.15)
project(chakra_control_edgehtml LANGUAGES CXX)

set(WEBVIEW_ENABLE_CHAKRACORE ON CACHE BOOL
    "Enable the ChakraCore host scripting API" FORCE)
set(WEBVIEW_BUILD_CHAKRA_EXAMPLE OFF)
set(WEBVIEW_USE_EDGE OFF CACHE BOOL "Use the EdgeHTML backend" FORCE)
set(WEBVIEW_USE_MSHTML OFF CACHE BOOL "Use the MSHTML backend" FORCE)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../.."
                 "${CMAKE_CURRENT_BINARY_DIR}/yaww")

add_executable(chakra_control_edgehtml WIN32
    "${CMAKE_CURRENT_SOURCE_DIR}/../chakra-control/main.cpp")
target_compile_features(chakra_control_edgehtml PRIVATE cxx_std_17)
target_link_libraries(chakra_control_edgehtml PRIVATE webview)
add_dependencies(chakra_control_edgehtml webview_chakracore_build)
set_target_properties(chakra_control_edgehtml PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_custom_command(TARGET chakra_control_edgehtml POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../chakra-control/webview.js"
        "$<TARGET_FILE_DIR:chakra_control_edgehtml>/webview.js"
    VERBATIM)
webview_deploy_chakracore(chakra_control_edgehtml)
