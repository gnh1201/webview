cmake_minimum_required(VERSION 3.15)
project(ipc_mshtml_example LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "The IPC MSHTML example is available only on Windows.")
endif()

set(WEBVIEW_USE_MSHTML ON CACHE BOOL "Use the MSHTML backend" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../ ${CMAKE_CURRENT_BINARY_DIR}/webview)

set(CMAKE_WIN32_EXECUTABLE ${WIN32})
add_executable(ipc_mshtml_example ../ipc/ipc.cpp)
yaww_add_version_info(ipc_mshtml_example)
target_compile_features(ipc_mshtml_example PRIVATE cxx_std_17)
target_link_libraries(ipc_mshtml_example PRIVATE webview comdlg32)
configure_file(index.html ${CMAKE_CURRENT_BINARY_DIR}/index.html COPYONLY)
