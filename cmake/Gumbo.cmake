include(FetchContent)
enable_language(C)

# Pin the Gumbo parser revision so every backend uses the same HTML parser.
FetchContent_Declare(gumbo_parser
  GIT_REPOSITORY https://codeberg.org/gumbo-parser/gumbo-parser.git
  GIT_TAG 6b414b60f83dc5d89970bbbd01b8f77cf7036eda
  GIT_SHALLOW FALSE
)
FetchContent_GetProperties(gumbo_parser)
if(NOT gumbo_parser_POPULATED)
  FetchContent_Populate(gumbo_parser)
endif()

set(WEBVIEW_GUMBO_SOURCES
  attribute.c char_ref.c char_ref_gperf.c error.c parser.c
  string_buffer.c string_piece.c tag.c tokenizer.c utf8.c util.c vector.c)
list(TRANSFORM WEBVIEW_GUMBO_SOURCES
  PREPEND "${gumbo_parser_SOURCE_DIR}/src/")

add_library(webview_gumbo STATIC ${WEBVIEW_GUMBO_SOURCES})
target_include_directories(webview_gumbo PUBLIC
  "${gumbo_parser_SOURCE_DIR}/src"
  "${gumbo_parser_SOURCE_DIR}/visualc/include")
