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
