if(NOT WIN32 OR NOT MSVC)
	message(FATAL_ERROR "WEBVIEW_ENABLE_CHAKRACORE currently requires Windows and MSVC")
endif()

include(FetchContent)
find_program(WEBVIEW_MSBUILD_EXECUTABLE NAMES MSBuild.exe msbuild REQUIRED)

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
set_property(TARGET webview PROPERTY WEBVIEW_CHAKRACORE_DLL
	"${CHAKRACORE_DLL}")
add_custom_target(webview_chakracore_build
	COMMAND "${WEBVIEW_MSBUILD_EXECUTABLE}"
		"${chakracore_SOURCE_DIR}/Build/Chakra.Core.sln"
		/m /p:Configuration=Release /p:Platform=${CHAKRACORE_PLATFORM}
	BYPRODUCTS "${CHAKRACORE_LIBRARY}" "${CHAKRACORE_DLL}"
	COMMENT "Building latest ChakraCore master for ${CHAKRACORE_PLATFORM}"
	VERBATIM)

target_include_directories(webview SYSTEM INTERFACE "${chakracore_SOURCE_DIR}/lib/Jsrt")
target_compile_definitions(webview INTERFACE WEBVIEW_ENABLE_CHAKRACORE)
target_link_libraries(webview INTERFACE "${CHAKRACORE_LIBRARY}" shell32)
add_dependencies(webview webview_chakracore_build)

function(webview_deploy_chakracore target)
	get_property(chakracore_runtime TARGET webview
		PROPERTY WEBVIEW_CHAKRACORE_DLL)
	add_custom_command(TARGET "${target}" POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"${chakracore_runtime}" "$<TARGET_FILE_DIR:${target}>/ChakraCore.dll"
		VERBATIM)
endfunction()
