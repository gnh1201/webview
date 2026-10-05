# Webview Examples

To build these examples, follow the install steps for your OS in the [top level README](./README.md).

`basic` opens `https://google.com`. `local-only` enables local-file-only
navigation, loads the bundled `index.html`, and shows `Forbidden` if you follow
its external link.

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
