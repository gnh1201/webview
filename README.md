# webview

A tiny C++ webview library focused on Windows. It provides backends for Microsoft Edge WebView2 (Chromium), EdgeHTML (legacy Edge), and MSHTML (Trident).

This repository is [forked from MichaelKim/webview](https://github.com/MichaelKim/webview) and builds on ideas from [zerge's webview](https://github.com/webview/webview). Recent development and testing have focused on Windows. macOS and Linux backends remain in the source, but have not been tested since the versions noted below and are not part of the current support matrix.

## Support

| Platform | Backend | Engine | Notes |
| --- | --- | --- | --- |
| Windows | WebView2 (`WEBVIEW_EDGE`) | Microsoft Edge (Chromium) | Recommended; requires the WebView2 Runtime. |
| Windows | EdgeHTML (`WEBVIEW_WIN`) | Microsoft Edge Legacy | Requires Windows 10 version 1809 or later; deprecated. |
| Windows | MSHTML (`WEBVIEW_MSHTML`) | Trident | Legacy compatibility only; requires ATL. |

The macOS WebKit backend was last listed as tested on macOS Mojave and Catalina. The Linux WebKitGTK backend was last listed as tested on Ubuntu 18.04.02 LTS. Neither has been tested against newer releases as part of recent Windows-focused work.

## WelsonJS and MSHTML

MSHTML support was added for [WelsonJS](https://github.com/gnh1201/welsonjs), a lightweight Windows JavaScript framework used to build applications and automate industrial systems. Its applications combine JavaScript (including transpiled languages) with HTML and CSS, and may need to run in legacy or resource-constrained Windows environments. The MSHTML backend allows those interfaces to use the Trident engine provided by the Windows system when a newer browser runtime is unavailable or unsuitable. Because MSHTML is deprecated, use it only when that compatibility is required.

The MSHTML implementation includes balanced COM/OLE initialization and cleanup, navigation and new-window event handling, defensive initialization and teardown, and JavaScript/native callback support. In local-file-only mode, external document and frame navigation is blocked and displays a built-in Forbidden page; subresource loads such as scripts and stylesheets are not filtered.

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

Include `webview.hpp` and link/configure the dependencies for the selected backend (see [Build Steps](docs/build.md)). By default, the Windows CMake configuration uses WebView2.

```cpp
#include "webview.hpp"

WEBVIEW_MAIN {
  wv::WebView webview{800, 600, true, true, Str("Hello world!")};
  webview.navigate(Str("https://google.com"));

  if (webview.init() == -1) {
    return 1;
  }

  while (webview.run() == 0)
    ;

  return 0;
}
```

To enable local-file-only document navigation, set the option before initialization:

```cpp
webview.setLocalFileOnly(true);
```

The local-only example demonstrates this option. On Windows, string values use `std::wstring`; wrap string literals with `Str("...")`. `WEBVIEW_MAIN` selects the appropriate Win32 or standard C++ entry point.

The library supports HTTP(S), local `file:///` URLs, and inline `data:` URLs. EdgeHTML does not support local file URLs; see [Limitations](docs/limitations.md).

## Documentation

- [Build Steps](docs/build.md)
- [API Reference](docs/api.md)
- [Limitations](docs/limitations.md)
- [Examples](examples/README.md)
