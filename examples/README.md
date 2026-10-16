# YAWW Examples

To build these examples, follow the install steps for your OS in the [top level README](./README.md).

`basic` opens `https://google.com`. `local-only` enables local-file-only
navigation, loads the bundled `index.html`, and shows `Forbidden` if you follow
its external link.

ChakraCore is available as an optional shared host-side API for `wv::WebView` on
WebView2, EdgeHTML, and MSHTML. Enable it with
`WEBVIEW_ENABLE_CHAKRACORE=ON`; this downloads ChakraCore's current `master`
branch and builds its Visual Studio solution, so network access and MSVC are
required. `wv::ChakraCore::Options` selects whether to start the runtime, the
script file to load, and whether to open the debug console. With no script path,
the runtime looks for `webview.js` beside the executable. An explicit script
path overrides that default. The same C++ API works with each Windows WebView
backend:

```powershell
cmake -S . -B out/build/chakra-control -DWEBVIEW_ENABLE_CHAKRACORE=ON -DWEBVIEW_BUILD_CHAKRA_EXAMPLE=ON
cmake --build out/build/chakra-control --config Debug --target chakra_control
out/build/chakra-control/bin/Debug/chakra_control.exe --chakra --chakra-script C:/path/to/control.js --chakra-console
```

The sample enables ChakraCore by default, so running it without arguments loads
the adjacent `webview.js`. At runtime, `--chakra-script` selects a different
script, `--chakra-console` opens the console at startup, and `--no-chakra`
disables the runtime. When ChakraCore is enabled,
the window's system menu also gains **Open ChakraCore Console**; open it by
right-clicking the title bar or pressing Alt+Space. Applications can configure
it directly:

```cpp
wv::ChakraCore::Options options;
options.enabled = true;
options.scriptPath = L"control.js"; // Leave empty to use adjacent webview.js.
options.openDebugConsole = false;
wv::ChakraCore chakra(view, options);
chakra.start();
// Open later when needed: chakra.showDebugConsole();
```

For application targets using CMake, link `webview` and call
`webview_deploy_chakracore(your_target)` to place the ChakraCore DLL beside the
executable.

The runtime exposes `webview.navigate()`, `webview.eval()`,
`webview.resizeTo()`, and `webview.on()` to the host script. `eval()` sends
JavaScript to the embedded page and does not return its result. The console's
Run button evaluates text in the host-side ChakraCore context. ChakraCore
provides JavaScript only; it does not provide a browser DOM. MSHTML requires
ATL in the Visual Studio C++ toolchain. Select the backend with the root
project's `WEBVIEW_USE_EDGE` and `WEBVIEW_USE_MSHTML` options.

`mshtml` is a Windows-only example that selects the MSHTML backend, loads a
local HTML page, demonstrates an HTML-to-native callback, and blocks external
document navigation. It requires the ATL component for the Visual Studio C++
toolchain. Configure and build it separately from the other examples:

```powershell
cmake -S examples/mshtml -B out/build/mshtml
cmake --build out/build/mshtml --config Debug
```

The example's CMake project enables `WEBVIEW_USE_MSHTML` automatically; it does
not use the default WebView2 backend. With a Visual Studio generator, add
`-A x64` to the configure command to select a 64-bit build.

`ipc` connects a YAWW window to a WelsonJS MCP process over stdio. Its button calls
`window.mcp.tools.call('evaluate_js', {script: 'return 1 + 2;', allowUnsafeEval: true})`
and displays the returned MCP result (`3`). It looks for `app.js` beside the example executable and
opens a file picker if the file is missing. Adjust the `cscript.exe` path in
`ipc.cpp` for your WelsonJS installation, then build it with:

```powershell
cmake -S examples/ipc -B out/build/ipc
cmake --build out/build/ipc --config Debug
```

Windows-only `ipc-mshtml` and `ipc-edgehtml` variants use the same process
connection and tool call with the respective legacy rendering engines. MSHTML
uses a callback because it has no native Promise support. The EdgeHTML variant
embeds its page as a `data:` URL because EdgeHTML cannot navigate to local HTML
files.

```powershell
cmake -S examples/ipc-mshtml -B out/build/ipc-mshtml
cmake --build out/build/ipc-mshtml --config Debug
cmake -S examples/ipc-edgehtml -B out/build/ipc-edgehtml
cmake --build out/build/ipc-edgehtml --config Debug
```

`open-file` accepts an `.hta`, `.htm`, or `.html` path. It builds a small
dispatcher and one viewer for each Windows backend. HTA files go to MSHTML;
HTML files use WebView2 when its runtime is available, then EdgeHTML when its
WinRT process API initializes, and otherwise MSHTML. This builds all three
engine backends, so the Visual Studio C++
toolchain must include ATL for the MSHTML viewer. The YAWW core reads each
page's `<title>` with Gumbo and updates the window title; the viewers do not
parse titles themselves. Configure and build all four executables together
(use a Visual Studio developer prompt so the C++ compiler is available):

```powershell
cmake -S examples/open-file -B out/build/open-file -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build out/build/open-file --config Debug
.\out\build\open-file\bin\open_file.exe "C:\path\to\page.html"
```

With a multi-configuration Visual Studio generator, the dispatcher is under
`bin/Debug` instead. Use `-A x64` when configuring with a Visual Studio
generator.

The EdgeHTML viewer reads HTML and loads it with `NavigateToString`, so
relative resources in the file are not resolved from the original directory.
Try the included samples from the repository root:

```powershell
.\out\build\open-file\bin\open_file.exe .\examples\open-file\samples\sample.html
.\out\build\open-file\bin\open_file.exe .\examples\open-file\samples\sample.hta
```

With a multi-configuration Visual Studio generator, use
`out/build/open-file/bin/Debug/open_file.exe` instead.

`ipc-open-file` combines stdio MCP IPC with the `open-file` engine selection.
It looks beside the executable for `app.js` and `index.html`, `index.hta`, or
`app.hta` in that order. When a required file is missing, it opens a file
picker. `.hta` documents use MSHTML; HTML documents use WebView2, EdgeHTML, or
MSHTML according to runtime availability. Both sample entry points are copied
beside the executable. The HTML sample is selected by default; pass a document
and optionally an `app.js` path to select files directly:

```powershell
cmake -S examples/ipc-open-file -B out/build/ipc-open-file -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build out/build/ipc-open-file --config Debug
.\out\build\ipc-open-file\bin\ipc_open_file.exe
.\out\build\ipc-open-file\bin\ipc_open_file.exe .\examples\ipc-open-file\samples\index.hta C:\path\to\app.js
```

With a multi-configuration Visual Studio generator, run
`out/build/ipc-open-file/bin/Debug/ipc_open_file.exe`. Its button calls
`evaluate_js` with `allowUnsafeEval: true`; adjust the `cscript.exe` selection in
the viewer if the WelsonJS installation requires a different Windows Script
Host architecture. HTML and HTA titles are handled by YAWW's common title
parser. Relative filesystem paths in HTA documents resolve from
the document directory. The MSHTML host allows ActiveX execution and scripting,
including `ADODB.Stream`. This policy applies to all documents rendered by the
MSHTML backend. The MSHTML backend requires ATL.

```sh
mkdir build
cd build

cmake ..
# or from the root directory
cmake ../examples/basic
# or
cmake ../examples/local-only

cmake --build .
```
