# Webview Examples

To build these examples, follow the install steps for your OS in the [top level README](./README.md).

`basic` opens `https://google.com`. `local-only` enables local-file-only
navigation, loads the bundled `index.html`, and shows `Forbidden` if you follow
its external link.

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

`ipc` connects the WebView to a WelsonJS MCP process over stdio. Its button calls
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
toolchain must include ATL for the MSHTML viewer. Configure and build all four
executables together (use a Visual Studio developer prompt so the C++ compiler
is available):

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
Host architecture. Relative filesystem paths in HTA documents resolve from
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
