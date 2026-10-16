cmake_minimum_required(VERSION 3.15)
project(open_file_webview2 LANGUAGES CXX)

set(WEBVIEW_USE_EDGE ON CACHE BOOL "Use the WebView2 backend" FORCE)
set(WEBVIEW_USE_MSHTML OFF CACHE BOOL "Use the MSHTML backend" FORCE)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OPEN_FILE_RUNTIME_OUTPUT_DIRECTORY}")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../ ${CMAKE_CURRENT_BINARY_DIR}/webview)

add_executable(open_file_webview2 WIN32 ../viewer.cpp)
yaww_add_version_info(open_file_webview2)
target_compile_features(open_file_webview2 PRIVATE cxx_std_17)
target_link_libraries(open_file_webview2 PRIVATE webview shell32 shlwapi)
add_custom_command(TARGET open_file_webview2 POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_BINARY_DIR}/WebView2Loader.dll"
    "$<TARGET_FILE_DIR:open_file_webview2>/WebView2Loader.dll")
