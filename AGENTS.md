# Repository guidance

## File paths and Unicode

- Treat file and directory names as exact filesystem identifiers. Preserve every character, including Korean characters, exactly as reported by the filesystem or repository tools.
- Never translate, transliterate, normalize, or replace Korean characters in a path with Chinese characters, Hanja, or visually similar characters. Preserve the original Unicode code points exactly.
- Do not guess an absolute checkout path. Prefer repository-relative paths in explanations and commands. If an absolute path is required, obtain it from the current working directory or a filesystem listing and copy it exactly.
- Before opening, editing, or linking to a path, verify that the exact path exists with a filesystem listing or repository search. If a path lookup fails, recheck the spelling from tool output instead of substituting similar-looking characters.
- When mentioning paths in Markdown or chat, use the exact spelling returned by the filesystem. Use code formatting for paths when a clickable link is not needed.

## Project structure and conventions

- YAWW is the documentation and product name. Keep the existing `webview` identifiers in C++ APIs, headers, CMake targets, and backend definitions unless a task explicitly asks to rename them.
- The main implementation is in `webview.hpp`; optional ChakraCore host scripting is implemented in `webview_chakra.hpp`.
- Windows backends are selected with the existing `WEBVIEW_EDGE`, `WEBVIEW_WIN`, and `WEBVIEW_MSHTML` definitions. Shared behavior should be implemented consistently across applicable backends.
- Examples live under `examples/`; build and backend instructions live under `docs/`. Keep examples and documentation aligned with API changes.
- Preserve compatibility with the legacy MSHTML engine when changing shared JavaScript or native APIs. MSHTML does not provide all modern JavaScript features, so check whether Promise, JSON, and modern syntax assumptions need an engine-specific path.
- Follow the surrounding C++ and CMake style. Keep changes focused on the requested behavior and avoid unrelated formatting or generated build output.

## Changes and verification

- Inspect the relevant implementation, example, and documentation before changing a shared API.
- For API changes, update the affected examples and documentation so they show the same names and behavior as the implementation.
- Run an appropriate build or verification command when requested, or when needed to establish that a code change works. Report what was run and any environment limitation.
- Do not commit changes unless explicitly asked.
