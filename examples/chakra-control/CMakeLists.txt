cmake_minimum_required(VERSION 3.15)

# Allow this example to be configured directly while still consuming the
# repository's WebView target and its selected backend compile definitions.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(chakra_control_example LANGUAGES CXX)
  set(WEBVIEW_BUILD_CHAKRA_EXAMPLE OFF)
  add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/../.."
    "${CMAKE_CURRENT_BINARY_DIR}/yaww")
endif()

include(FetchContent)
find_program(WEBVIEW_MSBUILD_EXECUTABLE NAMES MSBuild.exe msbuild REQUIRED)

# Track the upstream branch so a fresh configure downloads its latest commit.
FetchContent_Declare(chakracore
  GIT_REPOSITORY https://github.com/chakra-core/ChakraCore.git
  GIT_TAG master
  GIT_SHALLOW FALSE
  UPDATE_DISCONNECTED FALSE
)
FetchContent_GetProperties(chakracore)
if(NOT chakracore_POPULATED)
  FetchContent_Populate(chakracore)
endif()

if(CMAKE_GENERATOR_PLATFORM MATCHES "^[Aa][Rr][Mm]64")
  message(FATAL_ERROR "The current ChakraCore Visual Studio solution integration supports x86/x64 only.")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(CHAKRACORE_PLATFORM x64)
else()
  set(CHAKRACORE_PLATFORM x86)
endif()

set(CHAKRACORE_OUTPUT_DIR
  "${chakracore_SOURCE_DIR}/Build/VcBuild/bin/${CHAKRACORE_PLATFORM}_release")
set(CHAKRACORE_LIBRARY "${CHAKRACORE_OUTPUT_DIR}/ChakraCore.lib")
set(CHAKRACORE_DLL "${CHAKRACORE_OUTPUT_DIR}/ChakraCore.dll")
add_custom_target(chakracore_build
  COMMAND "${WEBVIEW_MSBUILD_EXECUTABLE}"
    "${chakracore_SOURCE_DIR}/Build/Chakra.Core.sln"
    /m /p:Configuration=Release /p:Platform=${CHAKRACORE_PLATFORM}
  BYPRODUCTS "${CHAKRACORE_LIBRARY}" "${CHAKRACORE_DLL}"
  COMMENT "Building latest ChakraCore master for ${CHAKRACORE_PLATFORM}"
  VERBATIM)

add_executable(chakra_control WIN32 main.cpp)
set_target_properties(chakra_control PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_compile_features(chakra_control PRIVATE cxx_std_17)
target_link_libraries(chakra_control PRIVATE webview "${CHAKRACORE_LIBRARY}")
target_include_directories(chakra_control PRIVATE "${chakracore_SOURCE_DIR}/lib/Jsrt")
add_dependencies(chakra_control chakracore_build)

add_custom_command(TARGET chakra_control POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CHAKRACORE_DLL}" "$<TARGET_FILE_DIR:chakra_control>/ChakraCore.dll"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/webview.js"
    "$<TARGET_FILE_DIR:chakra_control>/webview.js")
