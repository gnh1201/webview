#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "webview.hpp"
#include <shellapi.h>
#include <shlwapi.h>

namespace {

struct StartupFiles {
    std::filesystem::path document;
    std::filesystem::path appJs;
};

StartupFiles inputFiles() {
    int count = 0;
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!arguments) throw std::runtime_error("could not read the command line");
    StartupFiles files;
    if (count >= 3) {
        files.document = std::filesystem::absolute(arguments[1]);
        files.appJs = std::filesystem::absolute(arguments[2]);
    }
    LocalFree(arguments);
    if (files.document.empty() || !std::filesystem::is_regular_file(files.document) ||
        files.appJs.empty() || !std::filesystem::is_regular_file(files.appJs))
        throw std::runtime_error("the selected document or app.js does not exist");
    return files;
}

std::string pathToUtf8(const std::filesystem::path& path) {
    return path.u8string();
}

std::string cscriptPath() {
    wchar_t windowsDirectory[MAX_PATH]{};
    const UINT length = GetWindowsDirectoryW(windowsDirectory, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
        throw std::runtime_error("could not locate the Windows directory");
    const std::filesystem::path windows(windowsDirectory);
    const auto wow64 = windows / L"SysWOW64" / L"cscript.exe";
    if (std::filesystem::is_regular_file(wow64)) return pathToUtf8(wow64);
    const auto system32 = windows / L"System32" / L"cscript.exe";
    if (std::filesystem::is_regular_file(system32)) return pathToUtf8(system32);
    throw std::runtime_error("could not find cscript.exe");
}

#if defined(WEBVIEW_WIN)
std::wstring loadHtml(const std::filesystem::path& file) {
    std::ifstream input(file, std::ios::binary);
    if (!input) throw std::runtime_error("could not read the HTML file");
    const std::string utf8((std::istreambuf_iterator<char>(input)),
                           std::istreambuf_iterator<char>());
    const int length = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(),
        static_cast<int>(utf8.size()), nullptr, 0);
    if (length <= 0) throw std::runtime_error("HTML file is not valid UTF-8");
    std::wstring html(static_cast<size_t>(length), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(),
                            static_cast<int>(utf8.size()), html.data(),
                            length) != length)
        throw std::runtime_error("could not decode the HTML file");
    return L"data:text/html," + html;
}
#else
std::wstring fileUrl(const std::filesystem::path& file) {
    DWORD length = 32768;
    std::vector<wchar_t> url(length);
    const HRESULT result = UrlCreateFromPathW(file.c_str(), url.data(), &length, 0);
    if (FAILED(result)) throw std::runtime_error("could not create a file URL");
    return std::wstring(url.data());
}

class HtmlSnapshot {
public:
    explicit HtmlSnapshot(const std::filesystem::path& source) {
        wchar_t tempName[MAX_PATH]{};
        const auto directory = source.parent_path();
        if (GetTempFileNameW(directory.c_str(), L"wvf", 0, tempName) == 0)
            throw std::runtime_error("could not create a temporary HTML file");

        path_ = tempName;
        path_.replace_extension(L".tmp.html");
        if (!MoveFileW(tempName, path_.c_str())) {
            DeleteFileW(tempName);
            throw std::runtime_error("could not prepare a temporary HTML path");
        }
        if (!CopyFileW(source.c_str(), path_.c_str(), FALSE)) {
            DeleteFileW(path_.c_str());
            throw std::runtime_error("could not prepare the HTA for MSHTML");
        }
    }

    ~HtmlSnapshot() { DeleteFileW(path_.c_str()); }
    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};
#endif

}  // namespace

WEBVIEW_MAIN {
    try {
        const auto files = inputFiles();
        const auto& file = files.document;
#if defined(WEBVIEW_WIN)
        const auto url = loadHtml(file);
        wv::WebView webview{1000, 720, true, true, Str("Open HTML (EdgeHTML)"), url};
#elif defined(WEBVIEW_MSHTML)
        // URLMON treats .hta URLs as downloadable applications. Give MSHTML
        // an .html copy so it renders the markup inside the view instead.
        std::unique_ptr<HtmlSnapshot> snapshot;
        const auto mshtmlFile = [&]() -> const std::filesystem::path& {
            std::wstring extension = file.extension().wstring();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                           [](wchar_t ch) {
                               return static_cast<wchar_t>(towlower(ch));
            });
            if (extension == L".hta") {
                snapshot = std::make_unique<HtmlSnapshot>(file);
                return snapshot->path();
            }
            return file;
        }();
        const auto url = fileUrl(mshtmlFile);
        wv::WebView webview{1000, 720, true, true, Str("Open file (MSHTML)"), url};
#else
        const auto url = fileUrl(file);
        wv::WebView webview{1000, 720, true, true, Str("Open HTML (WebView2)"), url};
#endif
        webview.connectProcess(cscriptPath(),
                               {"/nologo", pathToUtf8(files.appJs), "mcploader",
                                "/quiet"});
        if (webview.init() == -1) return 1;
        while (webview.run() == 0) {}
        return 0;
    } catch (const std::exception& error) {
        const std::string errorText(error.what());
        const int length = MultiByteToWideChar(CP_UTF8, 0, errorText.c_str(), -1,
                                              nullptr, 0);
        std::wstring message(length > 0 ? static_cast<size_t>(length) : 0,
                             L'\0');
        if (length > 0) {
            MultiByteToWideChar(CP_UTF8, 0, errorText.c_str(), -1,
                                message.data(), length);
            message.resize(static_cast<size_t>(length - 1));
        }
        MessageBoxW(nullptr, message.c_str(), L"Open file", MB_OK | MB_ICONERROR);
        return 1;
    }
}
