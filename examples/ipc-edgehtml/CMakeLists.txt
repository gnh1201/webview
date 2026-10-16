cmake_minimum_required(VERSION 3.15)
project(ipc_edgehtml_example LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "The IPC EdgeHTML example is available only on Windows.")
endif()

set(WEBVIEW_USE_EDGE OFF CACHE BOOL "Use the WebView2 backend" FORCE)
set(WEBVIEW_USE_MSHTML OFF CACHE BOOL "Use the MSHTML backend" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../ ${CMAKE_CURRENT_BINARY_DIR}/webview)

set(CMAKE_WIN32_EXECUTABLE ${WIN32})
add_executable(ipc_edgehtml_example ipc.cpp)
yaww_add_version_info(ipc_edgehtml_example)
target_compile_features(ipc_edgehtml_example PRIVATE cxx_std_17)
target_compile_definitions(ipc_edgehtml_example PRIVATE
  _SILENCE_EXPERIMENTAL_COROUTINE_DEPRECATION_WARNINGS)
target_link_libraries(ipc_edgehtml_example PRIVATE webview comdlg32)
configure_file(index.html ${CMAKE_CURRENT_BINARY_DIR}/index.html COPYONLY)
