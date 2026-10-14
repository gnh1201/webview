#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <objbase.h>
#include <winrt/Windows.Web.UI.Interop.h>

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <string>
#include <vector>

namespace {

std::wstring quoteArgument(const std::wstring& value) {
    std::wstring quoted = L"\"";
    size_t backslashes = 0;
    for (const wchar_t ch : value) {
        if (ch == L'\\') {
            ++backslashes;
        } else if (ch == L'"') {
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
    quoted += L'"';
    return quoted;
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
                  const std::filesystem::path& file) {
    std::wstring commandLine = quoteArgument(executable.native()) + L" " +
                               quoteArgument(file.native());
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(), commandLine.data(), nullptr, nullptr,
                        FALSE, 0, nullptr, file.parent_path().c_str(),
                        &startup, &process)) {
        MessageBoxW(nullptr, L"Could not start the selected WebView engine.",
                    L"Open file", MB_OK | MB_ICONERROR);
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
    if (!arguments || count < 2) {
        MessageBoxW(nullptr, L"Usage: open_file.exe <file.html|file.hta>",
                    L"Open file", MB_OK | MB_ICONINFORMATION);
        if (arguments) LocalFree(arguments);
        return 2;
    }

    std::filesystem::path file = std::filesystem::absolute(arguments[1]);
    LocalFree(arguments);
    if (!std::filesystem::is_regular_file(file)) {
        MessageBoxW(nullptr, L"The specified file does not exist.", L"Open file",
                    MB_OK | MB_ICONERROR);
        return 2;
    }

    std::wstring extension = file.extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](wchar_t ch) { return static_cast<wchar_t>(towlower(ch)); });

    std::vector<wchar_t> modulePath(32768, L'\0');
    const DWORD moduleLength = GetModuleFileNameW(
        nullptr, modulePath.data(), static_cast<DWORD>(modulePath.size()));
    if (moduleLength == 0 || moduleLength >= modulePath.size()) return 2;
    const auto directory = std::filesystem::path(modulePath.data()).parent_path();

    std::filesystem::path viewer;
    if (extension == L".hta") {
        viewer = directory / L"open_file_mshtml.exe";
    } else if (extension == L".html" || extension == L".htm") {
        if (webView2RuntimeAvailable(directory / L"WebView2Loader.dll"))
            viewer = directory / L"open_file_webview2.exe";
        else if (edgeHtmlAvailable())
            viewer = directory / L"open_file_edgehtml.exe";
        else
            viewer = directory / L"open_file_mshtml.exe";
    } else {
        MessageBoxW(nullptr, L"Only .hta, .htm, and .html files are supported.",
                    L"Open file", MB_OK | MB_ICONERROR);
        return 2;
    }

    return launchViewer(viewer, file) ? 0 : 1;
}
