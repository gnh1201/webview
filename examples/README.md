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
