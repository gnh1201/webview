// Demonstrates the legacy MSHTML backend with a local page and native callback.
#include <filesystem>

#include "webview.hpp"

void callback(wv::WebView& webview, wv::String&) {
    webview.eval(Str("document.getElementById('result').innerText = '")
                 Str("Native callback received'"));
}

WEBVIEW_MAIN {
    const auto cwd = std::filesystem::current_path();
    wv::WebView webview{800,
                        600,
                        true,
                        true,
                        Str("MSHTML example"),
                        Str("file:///") + wv::String(cwd / "index.html")};
    webview.setLocalFileOnly(true);
    webview.setCallback(callback);

    if (webview.init() == -1) {
        return 1;
    }

    while (webview.run() == 0)
        ;

    return 0;
}
