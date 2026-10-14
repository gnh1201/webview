// Demonstrates window.external.invoke() and eval() with the MSHTML backend.
#include "webview.hpp"

void callback(wv::WebView& webview, wv::String& arg) {
    if (arg == Str("eval")) {
        webview.eval(Str(
            "alert('boo!');"
            "document.getElementById('result').innerText = 'Native callback completed.';"));
    }
}

WEBVIEW_MAIN {
    wv::WebView webview{800, 600, true, true, Str("MSHTML example")};
    webview.setLocalFileOnly(true);

    if (webview.init() == -1) {
        return 1;
    }

    // Register after initialization, matching the public API usage example.
    webview.setCallback(callback);

    while (webview.run() == 0)
        ;

    return 0;
}
