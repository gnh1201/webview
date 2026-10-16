# YAWW (Yet Another Windows WebView)

[![Discord chat](https://img.shields.io/discord/359930650330923008?logo=discord&utm_source=welsonjs)](https://discord.gg/XKG5CjtXEj?utm_source=welsonjs)

YAWW (Yet Another Windows WebView) is a small C++ library focused on embedding web content in Windows applications. It provides backends for Microsoft Edge WebView2 (Chromium), EdgeHTML (legacy Edge), and MSHTML (Trident). The existing C++ API, header, and build identifiers retain their `webview` names.

YAWW is forked from [MichaelKim's webview](https://github.com/MichaelKim/webview), builds on ideas from [zerge's webview](https://github.com/webview/webview), and draws inspiration from [litehtml](https://github.com/litehtml/litehtml)'s use of the [Gumbo parser](https://codeberg.org/gumbo-parser/gumbo-parser) and Microsoft's [mshta HTML Application host](https://learn.microsoft.com/en-us/previous-versions/ms536496%28v%3Dvs.85%29). Recent development and testing have focused on Windows. macOS and Linux backends remain in the source, but have not been tested since the versions noted below and are not part of the current support matrix.

## Support

| Platform | Backend | Engine | Notes |
| --- | --- | --- | --- |
| Windows | WebView2 (`WEBVIEW_EDGE`) | Microsoft Edge (Chromium) | Recommended; requires the WebView2 Runtime. |
| Windows | EdgeHTML (`WEBVIEW_WIN`) | Microsoft Edge Legacy | Requires Windows 10 version 1809 or later; deprecated. |
| Windows | MSHTML (`WEBVIEW_MSHTML`) | Trident | Industrial and legacy application compatibility; requires ATL. |

The macOS WebKit backend was last listed as tested on macOS Mojave and Catalina. The Linux WebKitGTK backend was last listed as tested on Ubuntu 18.04.02 LTS. Neither has been tested against newer releases as part of recent Windows-focused work.

## Shared local-file-only mode

`setLocalFileOnly(true)` is a common feature available in all three Windows backends: MSHTML, EdgeHTML, and WebView2. It is intended for applications that should display local pages rather than external websites, while still allowing those pages to reference external resources such as scripts and stylesheets. Attempts to navigate the main document or a frame to an external site are blocked and show a built-in Forbidden page. This controls page navigation; it is not a network sandbox and does not filter subresource requests.

## WelsonJS and MSHTML

MSHTML support was added to help [WelsonJS](https://github.com/gnh1201/welsonjs), a lightweight Windows JavaScript framework for application development and industrial-system automation, maintain compatibility with existing industrial systems and their web-based interfaces. WelsonJS applications combine JavaScript (including transpiled languages) with HTML and CSS. The MSHTML backend provides access to the system's Trident engine for environments where compatibility with existing applications and workflows is important.

The MSHTML implementation includes balanced COM/OLE initialization and cleanup, navigation and new-window event handling, defensive initialization and teardown, and JavaScript/native callback support.

See [MSHTML build instructions](docs/build.md#mshtml-trident) and the [`mshtml` example](examples/mshtml/).

## Build and examples

See [Build Steps](docs/build.md) for backend requirements and configuration. For a Visual Studio x64 WebView2 build, configure with `-A x64`; use `-A Win32` for 32-bit. The CMake configuration selects the WebView2 loader for the chosen architecture.

Windows example executables embed YAWW file and product descriptions and
version metadata. Set the release version in [`cmake/Version.cmake`](cmake/Version.cmake).

The MCP stdio API uses [nlohmann/json](https://github.com/nlohmann/json) for JSON-RPC parsing and serialization. CMake uses an installed 3.12.0+ package or fetches that release during configuration.

Examples are in [`examples/`](examples/):

- `basic` opens `https://google.com`.
- `local-only` loads `index.html` from the current directory and displays Not Found if it is missing. External page navigation is blocked with a Forbidden page.
- `mshtml` demonstrates the legacy MSHTML backend, local content, and a JavaScript-to-native callback.
- `ipc` demonstrates callback-based `window.mcp.tools.call('evaluate_js', params, callback)` over stdio JSON-RPC; the same example works with Promise-based engines.
- `ipc-mshtml` and `ipc-edgehtml` run MCP IPC on the legacy Windows engines; MSHTML uses a callback because it has no native Promise support.
- `open-file` is a Windows example that routes `.hta` files to MSHTML and `.html` files to WebView2, EdgeHTML, or MSHTML according to runtime availability.

YAWW handles HTML `<title>` and `<link rel="icon">` elements as common
features. It parses the document head with
[Gumbo](https://codeberg.org/gumbo-parser/gumbo-parser) and updates the native
window title and icon across backends;
Windows local files are also parsed before navigation, including the temporary
HTML copy used for HTA files.

The local-only setting applies to document navigation, not to resources referenced by the page. Do not treat it as a network sandbox for untrusted HTML.

## TODO

- Complete HTA (`.hta`) compatibility semantics. The `open-file` example routes `.hta` files to MSHTML, but does not reproduce standalone `mshta.exe` window and privilege behavior. HTA content has different security assumptions from normal browser content, so document that behavior before treating it as a full HTA host. See Microsoft's documentation for the [`HTA:APPLICATION` element](https://learn.microsoft.com/en-us/previous-versions/ms536476%28v%3Dvs.85%29) and [HTA security model](https://learn.microsoft.com/en-us/openspecs/ie_standards/ms-html401e/1cc72255-6605-4e5a-b401-37da1c365a56).

## Usage

Include `webview.hpp` and configure the dependencies for your backend (see
[Build Steps](docs/build.md)). By default, Windows uses WebView2.

```cpp
#include "webview.hpp"

void callback(wv::WebView& webview, wv::String& arg) {
  if (arg == Str("hello"))
    webview.eval(Str("alert('Hello from C++!')"));
}

WEBVIEW_MAIN {
  wv::WebView webview{800, 600, true, true, Str("Local files only")};
  webview.setLocalFileOnly(true);
  webview.setCallback(callback);

  // To load a website instead, disable local-only mode and set a URL:
  // webview.setLocalFileOnly(false);
  // webview.navigate(Str("https://google.com"));

  if (webview.init() == -1) {
    return 1;
  }

  while (webview.run() == 0)
    ;

  return 0;
}
```

With local-file-only mode enabled, the app loads `index.html` from its current
working directory and blocks external document navigation. Place this file in
that directory before running the app:

```html
<!doctype html>
<html>
<head><meta charset="utf-8"><title>Local WebView</title></head>
<body>
  <button onclick="window.external.invoke('hello')">
    Call native code
  </button>
</body>
</html>
```

On Windows, strings use `std::wstring`; wrap string literals with `Str("...")`.
`WEBVIEW_MAIN` selects the appropriate Win32 or standard C++ entry point.

The library supports HTTP(S), local `file:///` URLs, and inline `data:` URLs. EdgeHTML does not support local file URLs; see [Limitations](docs/limitations.md).

## Documentation

- [Build Steps](docs/build.md)
- [API Reference](docs/api.md)
- [Limitations](docs/limitations.md)
- [Examples](examples/README.md)

## MSHTML compatibility test

MSHTML compatibility was exercised with an `mshta`-style industrial application scenario, and the application behaved normally during testing:

![MSHTML compatibility test in an mshta-style industrial application, test page](https://catswords.blob.core.windows.net/welsonjs/images/screenshot9.png?utm_source=welsonjs)

![MSHTML compatibility test in an mshta-style industrial application, UI components page](https://catswords.blob.core.windows.net/welsonjs/images/screenshot10.png?utm_source=welsonjs)
