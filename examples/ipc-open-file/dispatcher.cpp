#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <objbase.h>
#include <winrt/Windows.Web.UI.Interop.h>

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <vector>

namespace {

std::wstring quoteArgument(const std::wstring& value) {
    std::wstring quoted = L"\"";
    size_t backslashes = 0;
    for (const wchar_t ch : value) {
        if (ch == L'\\') {
            ++backslashes;
        } else if (ch == L'\"') {
            quoted.append(backslashes * 2 + 1, L'\\');
            quoted += ch;
            backslashes = 0;
        } else {
            quoted.append(backslashes, L'\\');
            backslashes = 0;
            quoted += ch;
        }
    }
    quoted.append(backslashes * 2, L'\\');
    quoted += L'\"';
    return quoted;
}

std::filesystem::path executableDirectory() {
    std::vector<wchar_t> buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                                            static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) return {};
    return std::filesystem::path(buffer.data()).parent_path();
}

std::filesystem::path chooseFile(const wchar_t* title,
                                 const wchar_t* filter) {
    std::vector<wchar_t> filename(32768, L'\0');
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = filename.data();
    dialog.nMaxFile = static_cast<DWORD>(filename.size());
    dialog.lpstrTitle = title;
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    if (!GetOpenFileNameW(&dialog)) return {};
    return std::filesystem::path(filename.data());
}

std::filesystem::path chooseAppJs() {
    static constexpr wchar_t filter[] =
        L"JavaScript files (*.js)\0*.js\0All files (*.*)\0*.*\0\0";
    return chooseFile(L"Locate WelsonJS app.js", filter);
}

std::filesystem::path chooseDocument() {
    static constexpr wchar_t filter[] =
        L"HTML and HTA files (*.html;*.htm;*.hta)\0*.html;*.htm;*.hta\0"
        L"All files (*.*)\0*.*\0\0";
    return chooseFile(L"Locate index.html, index.hta, or app.hta", filter);
}

bool hasExtension(const std::filesystem::path& path,
                  std::initializer_list<const wchar_t*> extensions) {
    std::wstring extension = path.extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](wchar_t ch) { return static_cast<wchar_t>(towlower(ch)); });
    return std::any_of(extensions.begin(), extensions.end(),
                       [&](const wchar_t* candidate) {
                           return extension == candidate;
                       });
}

bool webView2RuntimeAvailable(const std::filesystem::path& loaderPath) {
    HMODULE loader = LoadLibraryW(loaderPath.c_str());
    if (!loader) return false;
    using GetAvailableVersion = HRESULT(STDAPICALLTYPE*)(PCWSTR, LPWSTR*);
    const auto getVersion = reinterpret_cast<GetAvailableVersion>(
        GetProcAddress(loader, "GetAvailableCoreWebView2BrowserVersionString"));
    LPWSTR version = nullptr;
    const bool available = getVersion &&
                           SUCCEEDED(getVersion(nullptr, &version)) && version;
    if (version) CoTaskMemFree(version);
    FreeLibrary(loader);
    return available;
}

bool edgeHtmlAvailable() {
    bool apartmentInitialized = false;
    try {
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        apartmentInitialized = true;
        {
            winrt::Windows::Web::UI::Interop::WebViewControlProcessOptions options;
            winrt::Windows::Web::UI::Interop::WebViewControlProcess process(options);
            (void)process.GetWebViewControls();
        }
        winrt::uninit_apartment();
        return true;
    } catch (...) {
        if (apartmentInitialized) winrt::uninit_apartment();
        return false;
    }
}

bool launchViewer(const std::filesystem::path& executable,
                  const std::filesystem::path& document,
                  const std::filesystem::path& appJs) {
    std::wstring commandLine = quoteArgument(executable.native()) + L" " +
                               quoteArgument(document.native()) + L" " +
                               quoteArgument(appJs.native());
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(), commandLine.data(),
                        nullptr, nullptr, FALSE, 0, nullptr,
                        document.parent_path().c_str(), &startup, &process)) {
        MessageBoxW(nullptr, L"Could not start the selected WebView engine.",
                    L"MCP IPC open-file", MB_OK | MB_ICONERROR);
        return false;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
}

}  // namespace

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    int count = 0;
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    const auto directory = executableDirectory();
    if (directory.empty()) {
        if (arguments) LocalFree(arguments);
        MessageBoxW(nullptr, L"Could not locate the example directory.",
                    L"MCP IPC open-file", MB_OK | MB_ICONERROR);
        return 2;
    }

    std::filesystem::path document;
    std::filesystem::path appJs;
    if (arguments && count > 1) document = std::filesystem::absolute(arguments[1]);
    if (arguments && count > 2) appJs = std::filesystem::absolute(arguments[2]);
    if (arguments) LocalFree(arguments);

    if (document.empty()) {
        const auto html = directory / L"index.html";
        const auto hta = directory / L"index.hta";
        const auto appHta = directory / L"app.hta";
        if (std::filesystem::is_regular_file(html)) document = html;
        else if (std::filesystem::is_regular_file(hta)) document = hta;
        else if (std::filesystem::is_regular_file(appHta)) document = appHta;
        else document = chooseDocument();
    }
    if (document.empty() || !std::filesystem::is_regular_file(document) ||
        !hasExtension(document, {L".html", L".htm", L".hta"})) {
        MessageBoxW(nullptr, L"Select an existing .html, .htm, or .hta file.",
                    L"MCP IPC open-file", MB_OK | MB_ICONERROR);
        return 2;
    }

    if (appJs.empty()) {
        const auto localAppJs = directory / L"app.js";
        if (std::filesystem::is_regular_file(localAppJs)) appJs = localAppJs;
        else appJs = chooseAppJs();
    }
    if (appJs.empty() || !std::filesystem::is_regular_file(appJs) ||
        !hasExtension(appJs, {L".js"})) {
        MessageBoxW(nullptr, L"Select an existing app.js JavaScript file.",
                    L"MCP IPC open-file", MB_OK | MB_ICONERROR);
        return 2;
    }

    std::vector<wchar_t> modulePath(32768, L'\0');
    const DWORD moduleLength = GetModuleFileNameW(
        nullptr, modulePath.data(), static_cast<DWORD>(modulePath.size()));
    if (moduleLength == 0 || moduleLength >= modulePath.size()) return 2;
    const auto binDirectory = std::filesystem::path(modulePath.data()).parent_path();

    std::filesystem::path viewer;
    if (hasExtension(document, {L".hta"})) {
        viewer = binDirectory / L"ipc_open_file_mshtml.exe";
    } else if (webView2RuntimeAvailable(binDirectory / L"WebView2Loader.dll")) {
        viewer = binDirectory / L"ipc_open_file_webview2.exe";
    } else if (edgeHtmlAvailable()) {
        viewer = binDirectory / L"ipc_open_file_edgehtml.exe";
    } else {
        viewer = binDirectory / L"ipc_open_file_mshtml.exe";
    }
    return launchViewer(viewer, document, appJs) ? 0 : 1;
}
