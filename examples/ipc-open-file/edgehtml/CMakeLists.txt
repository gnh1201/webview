cmake_minimum_required(VERSION 3.15)
project(ipc_open_file_edgehtml LANGUAGES CXX)

set(WEBVIEW_USE_EDGE OFF CACHE BOOL "Use the WebView2 backend" FORCE)
set(WEBVIEW_USE_MSHTML OFF CACHE BOOL "Use the MSHTML backend" FORCE)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${IPC_OPEN_FILE_RUNTIME_OUTPUT_DIRECTORY}")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../ ${CMAKE_CURRENT_BINARY_DIR}/webview)

add_executable(ipc_open_file_edgehtml WIN32 ../viewer.cpp)
yaww_add_version_info(ipc_open_file_edgehtml)
target_compile_features(ipc_open_file_edgehtml PRIVATE cxx_std_17)
target_compile_definitions(ipc_open_file_edgehtml PRIVATE
  _SILENCE_EXPERIMENTAL_COROUTINE_DEPRECATION_WARNINGS)
target_link_libraries(ipc_open_file_edgehtml PRIVATE webview shell32 shlwapi)
