cmake_minimum_required(VERSION 3.15)
project(open_file_mshtml LANGUAGES CXX)

set(WEBVIEW_USE_EDGE OFF CACHE BOOL "Use the WebView2 backend" FORCE)
set(WEBVIEW_USE_MSHTML ON CACHE BOOL "Use the MSHTML backend" FORCE)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OPEN_FILE_RUNTIME_OUTPUT_DIRECTORY}")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../ ${CMAKE_CURRENT_BINARY_DIR}/webview)

add_executable(open_file_mshtml WIN32 ../viewer.cpp)
target_compile_features(open_file_mshtml PRIVATE cxx_std_17)
target_link_libraries(open_file_mshtml PRIVATE webview shell32 shlwapi)
