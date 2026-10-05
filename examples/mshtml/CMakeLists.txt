cmake_minimum_required(VERSION 3.15)
project(mshtml_example LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "The MSHTML example is available only on Windows.")
endif()

set(WEBVIEW_USE_MSHTML ON CACHE BOOL "Use the MSHTML backend" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../ ${CMAKE_CURRENT_BINARY_DIR}/webview)

set(CMAKE_WIN32_EXECUTABLE ${WIN32})

add_executable(mshtml_example mshtml.cpp)
target_compile_features(mshtml_example PRIVATE cxx_std_17)
target_link_libraries(mshtml_example PRIVATE webview)
add_custom_command(TARGET mshtml_example POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_CURRENT_SOURCE_DIR}/index.html"
          "$<TARGET_FILE_DIR:mshtml_example>/index.html")
if(MSVC)
  set_property(TARGET mshtml_example PROPERTY VS_DEBUGGER_WORKING_DIRECTORY
               "${CMAKE_CURRENT_BINARY_DIR}")
endif()
