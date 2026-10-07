# Build Steps

## Table of Contents

- Windows
  - [EdgeHTML](#edgehtml-edge-legacy) (Microsoft Edge Legacy)
  - [Chromium](#chromium-edge) (Microsoft Edge, recommended)
- [MacOS](#macos)
- [Linux](#linux)
- [Unit Tests](#unit-tests)

If you have CMake installed, the included config should work for all platforms.

## EdgeHTML (Edge Legacy)

_Note: It's highly recommended to choose the new Chromium-based Edge over the EdgeHTML-based Edge Legacy._

Define `WEBVIEW_WIN` before adding `webview.hpp`.

In order to target EdgeHTML (Microsoft Edge Legacy), `webview` uses the new C++/WinRT API.

### Requirements

- Visual Studio 2019
  - [C++/WinRT Visual Studio Extension](https://marketplace.visualstudio.com/items?itemName=CppWinRTTeam.cppwinrt101804264)
  - I haven't tested previous versions, but 2015 and 2017 should work as well.
- At least Windows 10, version 1809 (Build 17763)
  - The C++/WinRT API is fairly new, and its webview was introduced in v1803 ([UniversalAPIContract v6](https://docs.microsoft.com/en-us/uwp/api/windows.web.ui.interop.webviewcontrol)). This also uses `WebViewControl.AddInitializeScript` introduced in v1809. For more information about API contracts, read this [blog post by Microsoft](https://blogs.windows.com/buildingapps/2015/09/15/dynamically-detecting-features-with-api-contracts-10-by-10/#sRg3eXXT8oJzhUxY.97).

A few things to note:

- To debug, install the [Microsoft Edge DevTools](https://www.microsoft.com/en-us/p/microsoft-edge-devtools-preview/9mzbfrmz0mnj).
- Displaying `localhost` in the webview will only work after adding a loopback exception. A simple way to enable this is to run

  ```pwsh
  CheckNetIsolation.exe LoopbackExempt -a -n=Microsoft.Win32WebViewHost_cw5n1h2txyewy
  ```

  This can then be checked using `CheckNetIsolation.exe LoopbackExempt -s`. Read more about network loopbacks [here](<https://docs.microsoft.com/en-us/previous-versions/windows/apps/dn640582(v=win.10)>).

<details><summary><strong>Build with cl.exe (Edge Legacy)</strong></summary>

To use `cl.exe` directly, you'd need to grab the NuGet packages manually.

1. Download / clone this repo and navigate to it.
2. Get the [C++/WinRT package](https://www.nuget.org/packages/Microsoft.Windows.CppWinRT/) either by using the [NuGet CLI](https://www.nuget.org/downloads) or downloading them from the NuGet website.
3. Run `.\bin\cppwinrt.exe -in sdk`. (Optionally, you can add the `-verbose` flag.)
   - This should generate a local directory called `winrt` containing a bunch of headers. These are WinRT projection headers that you can use to consume from C++ code. You will be needing these headers for compilation.
4. Compile by running `cl main.cpp /DWEBVIEW_WIN /EHsc /I "." /std:c++17 /link WindowsApp.lib user32.lib kernel32.lib`.

If your `winrt` directory is located somewhere else, change the `/I "."` argument above.

</details>

<details><summary><strong>Build with Clang (Edge Legacy)</strong></summary>

While not officially supported, Microsoft does use Clang internally for testing purposes. If you want to use Clang, they have some basic instructions on <a href="https://docs.microsoft.com/en-us/windows/uwp/cpp-and-winrt-apis/faq#can-i-use-llvm-clang-to-compile-with-c---winrt-" rel="nofollow">their website</a>.

I've gotten `clang-cl` to compile with the following steps:

1. Download / clone this repo and navigate to it.
2. Get the [C++/WinRT package](https://www.nuget.org/packages/Microsoft.Windows.CppWinRT/) either by using the [NuGet CLI](https://www.nuget.org/downloads) or downloading them from the NuGet website.
3. Run `.\bin\cppwinrt.exe -in sdk`. (Optionally, you can add the `-verbose` flag.)
4. Install LLVM 8.0.0. (I've tested it with 8.0.0, but Microsoft says LLVM 6.0.0 should work too.)
   - (Optional) Add LLVM to your PATH, specifically `clang-cl.exe`.
5. Compile by running `clang-cl main.cpp /DWEBVIEW_WIN /EHsc /I "." -Xclang -std=c++17 -Xclang -Wno-delete-non-virtual-dtor /link "WindowsApp.lib" "user32.lib" "kernel32.lib"`.

If your `winrt` directory is located somewhere else, change the `/I "."` argument above.

</details>

## Chromium (Edge)

Define `WEBVIEW_EDGE` before adding `webview.hpp`.

To target the new Chromium Edge, `webview` uses the new WebView2 SDK.

Follow the instructions in the [official WebView2 docs](https://docs.microsoft.com/en-us/microsoft-edge/webview2/gettingstarted/win32).

To summarize:

- Visual Studio 2019 or later
- Windows versions supported by the installed [WebView2 Runtime](https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/distribution)
- Microsoft Edge (Chromium)
  - Install the [WebView2 Runtime](https://developer.microsoft.com/en-us/microsoft-edge/webview2/#download-section) or a Microsoft Edge installation that provides the runtime
- Add the following NuGet packages to the solution:
  - [Microsoft.Windows.ImplementationLibrary](https://www.nuget.org/packages/Microsoft.Windows.ImplementationLibrary/)
  - [Microsoft.Web.WebView2](https://www.nuget.org/packages/Microsoft.Web.WebView2)

The CMake configuration selects the WebView2 loader matching the target architecture. To generate a 32-bit Visual Studio build, pass `-A Win32`; for 64-bit, pass `-A x64`.

<details><summary><strong>Build with cl.exe (Edge Chromium)</strong></summary>

To use `cl.exe` directly, you'd need to grab the NuGet packages manually.

1. Download / clone this repo and navigate to it.
2. Make sure the WebView2 Runtime is installed.
3. Get the WebView2 package and the Windows Implementation Libraries (WIL) package either by using the [NuGet CLI](https://www.nuget.org/downloads) or downloading them from the NuGet website.
   - From WebView2, use the headers in `.\build\native\include` and the loader files under the folder matching your target architecture: `x86`, `x64`, or `arm64`.
     - For static linking, use that folder's `WebView2LoaderStatic.lib`.
     - For dynamic linking, use `WebView2Loader.dll.lib` and place the matching `WebView2Loader.dll` beside the executable when running.
   - From WIL, you need `.\include\wil\`.
4. Compile by running `cl main.cpp /DWEBVIEW_EDGE /EHsc /std:c++17 /link WebView2LoaderStatic.lib version.lib`, using the loader library for the selected target architecture.

</details>

WebView2 background colors support only fully transparent (`a = 0`) or fully opaque (`a = 255`) alpha; intermediate alpha values are treated as opaque.

## MSHTML (Trident)

MSHTML is available as a legacy Windows backend for applications that need the system Trident rendering engine. It uses the ATL ActiveX host, so install the ATL component for your Visual Studio C++ toolchain. This backend is deprecated by Microsoft and uses the document mode configured for the hosting application.

With CMake, configure with `-DWEBVIEW_USE_MSHTML=ON`. This selects `WEBVIEW_MSHTML` and takes precedence over the default Chromium Edge option. For a manual build, define `WEBVIEW_MSHTML` and link `ole32.lib`, `oleaut32.lib`, and `uuid.lib`.

The backend implements navigation, JavaScript evaluation, CSS injection, window callbacks, title and window sizing. JavaScript can call `window.external.invoke(...)` to reach the native callback through the ATL host's external `IDispatch` object.

See the Windows-only [`mshtml` example](../examples/mshtml/) for a local HTML page, a native callback, and local-file-only navigation. Its CMake project selects MSHTML automatically.

## MacOS

webview depends on the Cocoa and Webkit frameworks. Also, make sure your compiler supports Objective-C++ (g++ and clang++ should both work).

Use the provided CMake config to compile. Otherwise, to manually compile,

- Define `WEBVIEW_MAC`
- Enable Objective-C++ compilation
- Add frameworks Cocoa and Webkit

For example,

```sh
clang++ main.cpp -DWEBVIEW_MAC -ObjC++ -std=c++11 -framework Cocoa -framework Webkit -o my_webview
```

## Linux

webview depends on `gtk+-3.0` and `webkit2gtk-4.0`:

```sh
sudo apt-get install libgtk-3-dev libwebkit2gtk-4.0-37 libwebkit2gtk-4.0-dev
```

Use the provided CMake config to compile. Otherwise, to manually compile,

- Define `WEBVIEW_GTK`
- Add `gtk+-3.0` and `webkit2gtk-4.0`

For example,

```sh
g++ main.cpp -DWEBVIEW_GTK `pkg-config --cflags --libs gtk+-3.0 webkit2gtk-4.0` -o my_webview
```

## Unit Tests

To build and run the tests,

```sh
mkdir build
cd build
cmake ../test
cmake --build .
ctest --timeout 5
```

Certain tests will load files from `localhost:8080`. Before running tests, make sure to locally host the `build` directory (or wherever CMake is configured). For example,

```sh
python3 -m http.server 8080 # Python 3

npm install -g http-server
http-server -p 8080         # Node
```

### Running Headless Tests

- Windows: Supported only for Chromium Edge (`WEBVIEW_EDGE`). Make sure to install the Windows 10 SDK and the WebView2 Runtime before running the webview.
- MacOS: Supported.
- Linux: You'll need to run `xvfb` (or similar) before running.

For CI examples, check out the Github Actions config under `.github/workflows/`.
