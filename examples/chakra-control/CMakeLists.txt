cmake_minimum_required(VERSION 3.15)

# Allow this example to be configured directly while still consuming the
# repository's WebView target and its selected backend compile definitions.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(chakra_control_example LANGUAGES CXX)
  set(WEBVIEW_BUILD_CHAKRA_EXAMPLE OFF)
  set(WEBVIEW_ENABLE_CHAKRACORE ON CACHE BOOL
      "Enable the ChakraCore host scripting API" FORCE)

  set(WEBVIEW_CHAKRA_BACKEND "webview2" CACHE STRING
      "WebView backend for the ChakraCore example (webview2, edgehtml, mshtml)")
  set_property(CACHE WEBVIEW_CHAKRA_BACKEND PROPERTY STRINGS
      webview2 edgehtml mshtml)
  string(TOLOWER "${WEBVIEW_CHAKRA_BACKEND}" WEBVIEW_CHAKRA_BACKEND_NORMALIZED)
  if(WEBVIEW_CHAKRA_BACKEND_NORMALIZED STREQUAL "webview2")
    set(WEBVIEW_USE_EDGE ON CACHE BOOL "Enables Edge Chromium for Windows" FORCE)
    set(WEBVIEW_USE_MSHTML OFF CACHE BOOL "Enables the legacy MSHTML engine for Windows" FORCE)
  elseif(WEBVIEW_CHAKRA_BACKEND_NORMALIZED STREQUAL "edgehtml")
    set(WEBVIEW_USE_EDGE OFF CACHE BOOL "Enables Edge Chromium for Windows" FORCE)
    set(WEBVIEW_USE_MSHTML OFF CACHE BOOL "Enables the legacy MSHTML engine for Windows" FORCE)
  elseif(WEBVIEW_CHAKRA_BACKEND_NORMALIZED STREQUAL "mshtml")
    set(WEBVIEW_USE_EDGE OFF CACHE BOOL "Enables Edge Chromium for Windows" FORCE)
    set(WEBVIEW_USE_MSHTML ON CACHE BOOL "Enables the legacy MSHTML engine for Windows" FORCE)
  else()
    message(FATAL_ERROR
      "WEBVIEW_CHAKRA_BACKEND must be webview2, edgehtml, or mshtml")
  endif()

  add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/../.."
    "${CMAKE_CURRENT_BINARY_DIR}/yaww")
endif()

add_executable(chakra_control WIN32 main.cpp)
set_target_properties(chakra_control PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_compile_features(chakra_control PRIVATE cxx_std_17)
target_link_libraries(chakra_control PRIVATE webview)
add_dependencies(chakra_control webview_chakracore_build)

add_custom_command(TARGET chakra_control POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/webview.js"
    "$<TARGET_FILE_DIR:chakra_control>/webview.js"
  VERBATIM)

webview_deploy_chakracore(chakra_control)
