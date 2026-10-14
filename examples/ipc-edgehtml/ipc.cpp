#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#include "webview.hpp"

#include <commdlg.h>

namespace {

std::filesystem::path executablePath() {
    std::vector<wchar_t> buffer(32768);
    const auto length = GetModuleFileNameW(
        nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length == buffer.size())
        throw std::runtime_error("could not locate the example executable");
    return std::filesystem::path(buffer.data(), buffer.data() + length);
}

std::filesystem::path chooseAppJs() {
    std::vector<wchar_t> filename(32768, L'\0');
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrFilter = L"JavaScript files (*.js)\0*.js\0All files (*.*)\0*.*\0\0";
    dialog.lpstrFile = filename.data();
    dialog.nMaxFile = static_cast<DWORD>(filename.size());
    dialog.lpstrTitle = L"Locate WelsonJS app.js";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    if (!GetOpenFileNameW(&dialog)) return {};
    return std::filesystem::path(filename.data());
}

std::string makeDataUrl(const std::filesystem::path& pagePath) {
    std::ifstream file(pagePath, std::ios::binary);
    if (!file) throw std::runtime_error("could not open the EdgeHTML example page");
    const std::string html((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
    // WEBVIEW_WIN recognizes this prefix and passes the rest to
    // WebViewControl::NavigateToString. Keep the HTML payload unescaped.
    return "data:text/html," + html;
}

std::wstring utf8ToWide(const std::string& value) {
    const int length = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0);
    if (length <= 0) throw std::runtime_error("could not decode the EdgeHTML page as UTF-8");
    std::wstring result(static_cast<size_t>(length), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                            static_cast<int>(value.size()), result.data(),
                            length) != length)
        throw std::runtime_error("could not convert the EdgeHTML page URL to UTF-16");
    return result;
}

}  // namespace

WEBVIEW_MAIN {
    std::filesystem::path executable;
    std::filesystem::path appJs;
    std::wstring pageUrl;
    try {
        executable = executablePath();
        appJs = executable.parent_path() / "app.js";
        if (!std::filesystem::is_regular_file(appJs)) {
            std::cerr << "app.js was not found beside the example. Select it in the file dialog.\n";
            appJs = chooseAppJs();
        }
        if (appJs.empty() || !std::filesystem::is_regular_file(appJs)) return 1;

        const auto page = makeDataUrl(executable.parent_path() / "index.html");
        pageUrl = utf8ToWide(page);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }

    wv::WebView webview{800, 500, true, true,
                        Str("MCP stdio IPC (EdgeHTML)"), pageUrl};
    try {
        webview.connectProcess("C:/Windows/SysWOW64/cscript.exe",
                               {"/nologo", appJs.u8string(), "mcploader", "/quiet"});
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }

    if (webview.init() == -1) return 1;
    while (webview.run() == 0) {}
    return 0;
}
