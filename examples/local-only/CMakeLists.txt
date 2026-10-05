cmake_minimum_required(VERSION 3.15)
project(local_only LANGUAGES CXX)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../ ${CMAKE_CURRENT_BINARY_DIR}/webview)

set(CMAKE_WIN32_EXECUTABLE ${WIN32})

configure_file(index.html index.html COPYONLY)

add_executable(local_only local_only.cpp)
target_compile_features(local_only PRIVATE cxx_std_17)
target_link_libraries(local_only PRIVATE webview)
if(MSVC)
  set_property(TARGET local_only PROPERTY VS_DEBUGGER_WORKING_DIRECTORY
               "${CMAKE_CURRENT_BINARY_DIR}")
endif()
