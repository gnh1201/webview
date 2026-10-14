#include <filesystem>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "webview.hpp"

#if defined(WEBVIEW_IS_WIN)
#include <commdlg.h>
#endif

#if defined(WEBVIEW_MAC)
#include <mach-o/dyld.h>
#elif !defined(WEBVIEW_IS_WIN)
#include <unistd.h>
#endif

namespace {

std::filesystem::path executablePath() {
#if defined(WEBVIEW_IS_WIN)
    std::vector<wchar_t> buffer(32768);
    const auto length = GetModuleFileNameW(nullptr, buffer.data(),
                                           static_cast<DWORD>(buffer.size()));
    if (length == 0 || length == buffer.size())
        throw std::runtime_error("could not locate the example executable");
    return std::filesystem::path(buffer.data(), buffer.data() + length);
#elif defined(WEBVIEW_MAC)
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buffer(size);
    if (_NSGetExecutablePath(buffer.data(), &size) != 0)
        throw std::runtime_error("could not locate the example executable");
    return std::filesystem::canonical(buffer.data());
#else
    std::vector<char> buffer(4096);
    const auto length = readlink("/proc/self/exe", buffer.data(), buffer.size());
    if (length <= 0 || static_cast<size_t>(length) == buffer.size())
        throw std::runtime_error("could not locate the example executable");
    return std::filesystem::path(std::string(buffer.data(), length));
#endif
}

std::filesystem::path chooseAppJs() {
#if defined(WEBVIEW_IS_WIN)
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
#elif defined(WEBVIEW_MAC)
    NSOpenPanel* panel = [NSOpenPanel openPanel];
    [panel setTitle:@"Locate WelsonJS app.js"];
    [panel setCanChooseFiles:YES];
    [panel setCanChooseDirectories:NO];
    [panel setAllowsMultipleSelection:NO];
    [panel setAllowedFileTypes:@[@"js"]];
    if ([panel runModal] != NSModalResponseOK) return {};
    const char* path = [[[panel URL] path] UTF8String];
    return path ? std::filesystem::u8path(path) : std::filesystem::path{};
#else
    if (gtk_init_check(nullptr, nullptr) == FALSE) return {};
    GtkWidget* dialog = gtk_file_chooser_dialog_new(
        "Locate WelsonJS app.js", nullptr, GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, nullptr);
    GtkFileFilter* filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "JavaScript files (*.js)");
    gtk_file_filter_add_pattern(filter, "*.js");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    std::filesystem::path result;
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (filename) {
            result = std::filesystem::u8path(filename);
            g_free(filename);
        }
    }
    gtk_widget_destroy(dialog);
    return result;
#endif
}

std::string pathToUtf8(const std::filesystem::path& path) {
    return path.u8string();
}

#if defined(WV_IPC_EDGEHTML)
std::string edgeHtmlDataUrl() {
    static constexpr char page[] = R"HTML(<!doctype html><html><head><meta charset="utf-8">
<title>MCP stdio IPC (EdgeHTML)</title></head><body>
<h1>MCP stdio IPC</h1><button id="run">Run evaluate_js</button>
<pre id="result">Click the button to evaluate JavaScript.</pre>
<script>document.getElementById('run').onclick=function(){
var result=document.getElementById('result');result.textContent='Running...';
window.mcp.tools.call('evaluate_js',{script:'1 + 2',allowUnsafeEval:true})
.then(function(value){result.textContent=JSON.stringify(value,null,2);},
function(error){result.textContent='Error: '+error.message;});};</script>
</body></html>)HTML";
    return std::string("data:text/html,") + page;
}
#endif

}  // namespace

WEBVIEW_MAIN {
    std::filesystem::path executable;
    std::filesystem::path appJs;
    try {
        executable = executablePath();
        appJs = executable.parent_path() / "app.js";
        if (!std::filesystem::is_regular_file(appJs)) {
            std::cerr << "app.js was not found beside the example. Select it in the file dialog.\n";
            appJs = chooseAppJs();
        }
        if (appJs.empty() || !std::filesystem::is_regular_file(appJs)) return 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }

#if defined(WV_IPC_EDGEHTML)
    const auto pageUrlAscii = edgeHtmlDataUrl();
    const std::wstring pageUrl(pageUrlAscii.begin(), pageUrlAscii.end());
    wv::WebView webview{800, 500, true, true, Str("MCP stdio IPC (EdgeHTML)"), pageUrl};
#else
    const auto page = executable.parent_path() / "index.html";
#if defined(WEBVIEW_IS_WIN)
    const auto pageUrl = std::wstring(L"file:///") + page.generic_wstring();
    wv::WebView webview{800, 500, true, true, Str("MCP stdio IPC"), pageUrl};
#else
    wv::WebView webview{800, 500, true, true, Str("MCP stdio IPC"),
                        std::string("file://") + page.generic_string()};
#endif
#endif

    try {
        webview.connectProcess("C:/Windows/SysWOW64/cscript.exe",
                               {"/nologo", pathToUtf8(appJs), "mcploader", "/quiet"});
    } catch (...) {
        return 1;
    }

    if (webview.init() == -1) return 1;
    while (webview.run() == 0) {}
    return 0;
}
