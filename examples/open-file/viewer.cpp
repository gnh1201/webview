#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

#include "webview.hpp"
#include <shellapi.h>
#include <shlwapi.h>

namespace {

std::filesystem::path inputFile() {
    int count = 0;
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!arguments) throw std::runtime_error("could not read the command line");
    std::filesystem::path file;
    if (count >= 2) file = std::filesystem::absolute(arguments[1]);
    LocalFree(arguments);
    if (file.empty() || !std::filesystem::is_regular_file(file))
        throw std::runtime_error("the specified HTML file does not exist");
    return file;
}

std::wstring htmlTitle(const std::filesystem::path& file) {
    std::ifstream input(file, std::ios::binary);
    if (!input) return {};
    const std::string bytes((std::istreambuf_iterator<char>(input)),
                            std::istreambuf_iterator<char>());
    if (bytes.empty()) return {};

    const int length = MultiByteToWideChar(
        CP_UTF8, 0, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
    if (length <= 0) return {};
    std::wstring html(static_cast<size_t>(length), L'\0');
    if (MultiByteToWideChar(CP_UTF8, 0, bytes.data(),
                            static_cast<int>(bytes.size()), html.data(),
                            length) != length)
        return {};

    static const std::wregex titlePattern(
        LR"(<title\b[^>]*>([\s\S]*?)</title\s*>)",
        std::regex_constants::icase);
    std::wsmatch match;
    if (!std::regex_search(html, match, titlePattern)) return {};

    std::wstring title = match[1].str();
    const std::pair<std::wstring, std::wstring> entities[] = {
        {L"&amp;", L"&"}, {L"&lt;", L"<"}, {L"&gt;", L">"},
        {L"&quot;", L"\""}, {L"&#39;", L"'"}, {L"&nbsp;", L" "}};
    for (const auto& entity : entities) {
        size_t position = 0;
        while ((position = title.find(entity.first, position)) !=
               std::wstring::npos) {
            title.replace(position, entity.first.size(), entity.second);
            position += entity.second.size();
        }
    }
    const auto first = title.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) return {};
    const auto last = title.find_last_not_of(L" \t\r\n");
    return title.substr(first, last - first + 1);
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
        const auto file = inputFile();
        const auto pageTitle = htmlTitle(file);
#if defined(WEBVIEW_WIN)
        const auto url = loadHtml(file);
        wv::WebView webview{1000, 720, true, true,
                            pageTitle.empty() ? Str("Open HTML (EdgeHTML)")
                                              : pageTitle,
                            url};
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
        wv::WebView webview{1000, 720, true, true,
                            pageTitle.empty() ? Str("Open file (MSHTML)")
                                              : pageTitle,
                            url};
#else
        const auto url = fileUrl(file);
        wv::WebView webview{1000, 720, true, true,
                            pageTitle.empty() ? Str("Open HTML (WebView2)")
                                              : pageTitle,
                            url};
#endif
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
