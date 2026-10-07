# webview

A tiny C++ webview library focused on Windows. It provides backends for Microsoft Edge WebView2 (Chromium), EdgeHTML (legacy Edge), and MSHTML (Trident).

This repository is [forked from MichaelKim/webview](https://github.com/MichaelKim/webview) and builds on ideas from [zerge's webview](https://github.com/webview/webview). Recent development and testing have focused on Windows. macOS and Linux backends remain in the source, but have not been tested since the versions noted below and are not part of the current support matrix.

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

Examples are in [`examples/`](examples/):

- `basic` opens `https://google.com`.
- `local-only` loads `index.html` from the current directory and displays Not Found if it is missing. External page navigation is blocked with a Forbidden page.
- `mshtml` demonstrates the legacy MSHTML backend, local content, and a JavaScript-to-native callback.

The local-only setting applies to document navigation, not to resources referenced by the page. Do not treat it as a network sandbox for untrusted HTML.

## TODO

- Add compatibility for the HTA (`.hta`) file format. HTA (HTML Application) files are HTML-based Windows applications, traditionally hosted by `mshta.exe`, and can include an `<HTA:APPLICATION>` element to configure application-window behavior such as its border. Compatibility should account for these application-specific semantics, rather than only opening the file as an ordinary HTML page. HTA content has different security assumptions from normal browser content, so this work must also define and document its security behavior. See Microsoft's documentation for the [`HTA:APPLICATION` element](https://learn.microsoft.com/en-us/previous-versions/ms536476%28v%3Dvs.85%29) and [HTA security model](https://learn.microsoft.com/en-us/openspecs/ie_standards/ms-html401e/1cc72255-6605-4e5a-b401-37da1c365a56).
- Add support for the inter-process communication (IPC) planned for the WelsonJS project.

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
