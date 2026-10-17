include(FetchContent)

FetchContent_Declare(scintilla
	URL https://www.scintilla.org/scintilla567.tgz
	URL_HASH SHA256=19E4476706750056F86CA70974810E4A5B08E73477E13F7D9FAE78243606F6A7
)
FetchContent_Declare(lexilla
	URL https://www.scintilla.org/lexilla554.tgz
	URL_HASH SHA256=FCD9EF74236A12359D1039827C93DBF827AA476C46F9FD6EEF60BB96DB1AF2F6
)

FetchContent_GetProperties(scintilla)
if(NOT scintilla_POPULATED)
	FetchContent_Populate(scintilla)
endif()
FetchContent_GetProperties(lexilla)
if(NOT lexilla_POPULATED)
	FetchContent_Populate(lexilla)
endif()

# FetchContent extracts each official archive directly into its source
# directory; the archive does not add another "scintilla" or "lexilla"
# directory level.
set(WEBVIEW_SCINTILLA_SOURCE_DIR "${scintilla_SOURCE_DIR}")
set(WEBVIEW_LEXILLA_SOURCE_DIR "${lexilla_SOURCE_DIR}")
if(NOT EXISTS "${WEBVIEW_SCINTILLA_SOURCE_DIR}/win32/scintilla.mak" OR
	NOT EXISTS "${WEBVIEW_LEXILLA_SOURCE_DIR}/src/lexilla.mak")
	message(FATAL_ERROR "The downloaded Scintilla/Lexilla source layout is unexpected")
endif()

# Lexilla's NMake file expects Scintilla's headers in a peer directory.
file(MAKE_DIRECTORY "${WEBVIEW_LEXILLA_SOURCE_DIR}/../scintilla")
file(COPY "${WEBVIEW_SCINTILLA_SOURCE_DIR}/include"
	DESTINATION "${WEBVIEW_LEXILLA_SOURCE_DIR}/../scintilla")

get_filename_component(WEBVIEW_MSVC_COMPILER_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
find_program(WEBVIEW_NMAKE_EXECUTABLE
	NAMES nmake.exe nmake
	HINTS "${WEBVIEW_MSVC_COMPILER_DIR}")
if(NOT WEBVIEW_NMAKE_EXECUTABLE)
	message(FATAL_ERROR
		"NMake was not found. Configure ChakraCore builds from a Visual Studio developer environment.")
endif()

set(WEBVIEW_SCINTILLA_DLL "${WEBVIEW_SCINTILLA_SOURCE_DIR}/bin/Scintilla.dll")
set(WEBVIEW_LEXILLA_DLL "${WEBVIEW_LEXILLA_SOURCE_DIR}/bin/lexilla.dll")
set(WEBVIEW_SCINTILLA_LICENSE "${WEBVIEW_SCINTILLA_SOURCE_DIR}/License.txt")
set(WEBVIEW_LEXILLA_LICENSE "${WEBVIEW_LEXILLA_SOURCE_DIR}/License.txt")
set_property(TARGET webview PROPERTY WEBVIEW_SCINTILLA_DLL "${WEBVIEW_SCINTILLA_DLL}")
set_property(TARGET webview PROPERTY WEBVIEW_LEXILLA_DLL "${WEBVIEW_LEXILLA_DLL}")
set_property(TARGET webview PROPERTY WEBVIEW_SCINTILLA_LICENSE
	"${WEBVIEW_SCINTILLA_LICENSE}")
set_property(TARGET webview PROPERTY WEBVIEW_LEXILLA_LICENSE
	"${WEBVIEW_LEXILLA_LICENSE}")

add_custom_target(webview_scintilla_build
	COMMAND "${WEBVIEW_NMAKE_EXECUTABLE}" /f scintilla.mak QUIET=1
	WORKING_DIRECTORY "${WEBVIEW_SCINTILLA_SOURCE_DIR}/win32"
	BYPRODUCTS "${WEBVIEW_SCINTILLA_DLL}"
	COMMENT "Building Scintilla 5.6.7"
	VERBATIM)
add_custom_target(webview_lexilla_build
	COMMAND "${WEBVIEW_NMAKE_EXECUTABLE}" /f lexilla.mak QUIET=1
	WORKING_DIRECTORY "${WEBVIEW_LEXILLA_SOURCE_DIR}/src"
	BYPRODUCTS "${WEBVIEW_LEXILLA_DLL}"
	COMMENT "Building Lexilla 5.5.4"
	VERBATIM)

target_include_directories(webview SYSTEM INTERFACE
	"${WEBVIEW_SCINTILLA_SOURCE_DIR}/include"
	"${WEBVIEW_LEXILLA_SOURCE_DIR}/include")
add_dependencies(webview webview_scintilla_build webview_lexilla_build)
