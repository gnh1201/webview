// Demonstrates window.external.invoke() and eval() with the MSHTML backend.
#include <filesystem>

#include "webview.hpp"

void callback(wv::WebView& webview, wv::String& arg) {
    if (arg == Str("eval")) {
        webview.eval(Str(
            "alert('boo!');"
            "document.getElementById('result').innerText = 'Native callback completed.';"));
    }
}

WEBVIEW_MAIN {
    std::vector<wchar_t> executablePath(32768);
    const DWORD executablePathLength = GetModuleFileNameW(
        nullptr, executablePath.data(),
        static_cast<DWORD>(executablePath.size()));
    if (executablePathLength == 0 ||
        executablePathLength >= executablePath.size()) {
        return 1;
    }
    const auto pagePath =
        std::filesystem::path(std::wstring(executablePath.data(),
                                           executablePathLength))
            .parent_path() /
        L"index.html";
    std::vector<wchar_t> pageUrl(32768);
    DWORD pageUrlLength = static_cast<DWORD>(pageUrl.size());
    if (FAILED(UrlCreateFromPathW(pagePath.c_str(), pageUrl.data(),
                                  &pageUrlLength, 0))) {
        return 1;
    }
    wv::WebView webview{800, 600, true, true, Str("MSHTML example"),
                        wv::String(pageUrl.data())};
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
