cmake_minimum_required(VERSION 3.15)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/Version.cmake)
project(ipc_open_file_example VERSION ${YAWW_VERSION} LANGUAGES CXX)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/VersionInfo.cmake)

if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

if(NOT WIN32)
  message(FATAL_ERROR "The IPC open-file example is available only on Windows.")
endif()

include(ExternalProject)
find_package(nlohmann_json 3.12.0 QUIET)
if(NOT TARGET nlohmann_json::nlohmann_json)
  include(FetchContent)
  set(JSON_BuildTests OFF CACHE INTERNAL "")
  FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
  FetchContent_MakeAvailable(nlohmann_json)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
set(IPC_OPEN_FILE_JSON_SOURCE_ARG)
if(DEFINED nlohmann_json_SOURCE_DIR)
  set(IPC_OPEN_FILE_JSON_SOURCE_ARG
    "-DFETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON:PATH=${nlohmann_json_SOURCE_DIR}")
endif()

add_executable(ipc_open_file WIN32 dispatcher.cpp)
yaww_add_version_info(ipc_open_file)
target_compile_features(ipc_open_file PRIVATE cxx_std_17)
target_include_directories(ipc_open_file PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../..)
target_compile_definitions(ipc_open_file PRIVATE
  _SILENCE_EXPERIMENTAL_COROUTINE_DEPRECATION_WARNINGS)
target_link_libraries(ipc_open_file PRIVATE shell32 comdlg32 windowsapp)

add_custom_command(TARGET ipc_open_file POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/samples/index.html"
    "$<TARGET_FILE_DIR:ipc_open_file>/index.html"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/samples/index.hta"
    "$<TARGET_FILE_DIR:ipc_open_file>/index.hta")

foreach(backend IN ITEMS mshtml edgehtml webview2)
  ExternalProject_Add(ipc_open_file_${backend}
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${backend}"
    BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/build-${backend}"
    CMAKE_GENERATOR "${CMAKE_GENERATOR}"
    CMAKE_ARGS
      "-DIPC_OPEN_FILE_RUNTIME_OUTPUT_DIRECTORY:PATH=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
      "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}"
      ${IPC_OPEN_FILE_JSON_SOURCE_ARG}
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config $<CONFIG>
    INSTALL_COMMAND ""
  )
  add_dependencies(ipc_open_file ipc_open_file_${backend})
endforeach()
