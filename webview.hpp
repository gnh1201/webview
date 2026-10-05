#ifndef WEBVIEW_H
#define WEBVIEW_H

#if !defined(WEBVIEW_WIN) && !defined(WEBVIEW_EDGE) && \
    !defined(WEBVIEW_MSHTML) && !defined(WEBVIEW_MAC) && !defined(WEBVIEW_GTK)
#error "Define one of WEBVIEW_WIN, WEBVIEW_EDGE, WEBVIEW_MSHTML, WEBVIEW_MAC, or WEBVIEW_GTK"
#endif

#if defined(WEBVIEW_WIN) || defined(WEBVIEW_EDGE) || defined(WEBVIEW_MSHTML)
#define WEBVIEW_IS_WIN
#endif

// Helper defines
#if defined(WEBVIEW_IS_WIN)
#define WEBVIEW_MAIN int __stdcall WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
#define GetProcNameAddress(hmod, proc) \
    reinterpret_cast<decltype(proc)*>(GetProcAddress(hmod, #proc))
#define UNICODE
#define _UNICODE
#define Str(s) L##s
#else
#define WEBVIEW_MAIN int main(int, char**)
#define Str(s) s
#endif

// Headers
#include <functional>
#include <cwchar>
#include <filesystem>
#include <iterator>
#include <string>

#if defined(WEBVIEW_MSHTML)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <exdisp.h>
#include <exdispid.h>
#include <mshtml.h>
#include <shellscalingapi.h>
#include <atlbase.h>
#include <atlcom.h>
#include <atlhost.h>
#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#pragma warning(push)
#pragma warning(disable : 4265)
class MshtmlNavigationSink
    : public CComObjectRootEx<CComSingleThreadModel>, public IDispatch {
public:
    std::function<void(BSTR, VARIANT_BOOL*)> beforeNavigate;

    BEGIN_COM_MAP(MshtmlNavigationSink)
    COM_INTERFACE_ENTRY(IDispatch)
    END_COM_MAP()

    STDMETHOD(GetTypeInfoCount)(UINT* count) override {
        if (!count) return E_POINTER;
        *count = 0;
        return S_OK;
    }
    STDMETHOD(GetTypeInfo)(UINT, LCID, ITypeInfo**) override {
        return E_NOTIMPL;
    }
    STDMETHOD(GetIDsOfNames)(REFIID, LPOLESTR*, UINT, LCID, DISPID*) override {
        return E_NOTIMPL;
    }
    STDMETHOD(Invoke)(DISPID id, REFIID, LCID, WORD, DISPPARAMS* params,
                      VARIANT*, EXCEPINFO*, UINT*) override {
        constexpr DISPID beforeNavigate2 = 250;
        constexpr DISPID newWindow2 = 251;
        constexpr DISPID newWindow3 = 273;
        if (!params) return S_OK;
        VARIANT* url = nullptr;
        VARIANT* cancelArg = nullptr;
        if (id == beforeNavigate2 && params->cArgs >= 7) {
            url = &params->rgvarg[5];
            cancelArg = &params->rgvarg[0];
        } else if (id == newWindow3 && params->cArgs >= 5) {
            url = &params->rgvarg[0];
            cancelArg = &params->rgvarg[3];
        } else if (id == newWindow2 && params->cArgs >= 2) {
            cancelArg = &params->rgvarg[0];
        } else {
            return S_OK;
        }
        VARIANT_BOOL* cancel =
            cancelArg->vt == (VT_BOOL | VT_BYREF) ? cancelArg->pboolVal : nullptr;
        const VARIANT* value = url;
        while (value && value->vt == (VT_VARIANT | VT_BYREF))
            value = value->pvarVal;
        BSTR target = nullptr;
        if (value && value->vt == VT_BSTR)
            target = value->bstrVal;
        else if (value && value->vt == (VT_BSTR | VT_BYREF) && value->pbstrVal)
            target = *value->pbstrVal;
        if (beforeNavigate) beforeNavigate(target, cancel);
        return S_OK;
    }
};
#pragma warning(pop)
#endif

#if defined(WEBVIEW_WIN)
#define WIN32_LEAN_AND_MEAN
#pragma comment(lib, "windowsapp")

#include <objbase.h>
#include <shellscalingapi.h>
#include <shlwapi.h>
#include <windows.h>
#include <winrt/Windows.Web.UI.Interop.h>

#include <memory>
#include <type_traits>
#include <utility>

#pragma warning(push)
#pragma warning(disable : 4265)
#include <winrt/Windows.Foundation.Collections.h>
#pragma warning(pop)
#elif defined(WEBVIEW_EDGE)  // WEBVIEW_WIN
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "user32.lib")
#include <WebView2.h>
#include <shellscalingapi.h>
#include <shlwapi.h>
#include <tchar.h>
#include <wil/com.h>
#include <windows.h>
#include <wrl.h>

#include <cstdlib>
#include <atomic>
#include <cwchar>
#include <memory>
#include <type_traits>
#include <utility>
#elif defined(WEBVIEW_MAC)  // WEBVIEW_EDGE
#import <Cocoa/Cocoa.h>
#import <Webkit/Webkit.h>
#include <objc/objc-runtime.h>

// ObjC declarations may only appear in global scope
@interface WindowDelegate : NSObject <NSWindowDelegate, WKScriptMessageHandler>
@end

@implementation WindowDelegate
- (void)userContentController:(WKUserContentController*)userContentController
      didReceiveScriptMessage:(WKScriptMessage*)scriptMessage {
}
@end
#elif defined(WEBVIEW_GTK)  // WEBVIEW_MAC
#include <JavaScriptCore/JavaScript.h>
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#endif

constexpr auto DEFAULT_URL = Str(R"(data:text/html,
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta http-equiv="X-UA-Compatible" content="IE=edge">
</head>
<body>
<div id="app"></div>
<script type="text/javascript"></script>
</body>
</html>)");

namespace wv {
// wv::String
#if defined(WEBVIEW_IS_WIN)
using String = std::wstring;
#else
using String = std::string;
#endif

// Namespaces
#if defined(WEBVIEW_WIN)
using namespace winrt::impl;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::Web::UI::Interop;
#elif defined(WEBVIEW_EDGE)
using namespace Microsoft::WRL;
#endif

#if defined(WEBVIEW_IS_WIN)
inline constexpr wchar_t FORBIDDEN_PAGE_URI[] =
    L"data:text/html,%3Ctitle%3EForbidden%3C/title%3E%3Ch1%3EForbidden%3C/h1%3E";
inline bool isLocalFileUri(const wchar_t* uri) {
    if (!uri) return false;
    size_t drive = 0;
    if (_wcsnicmp(uri, L"file:///", 8) == 0) {
        drive = 8;
    } else if (_wcsnicmp(uri, L"file://localhost/", 17) == 0) {
        drive = 17;
    } else {
        return false;
    }
    const auto length = wcslen(uri);
    return length >= drive + 3 &&
           ((uri[drive] >= L'A' && uri[drive] <= L'Z') ||
            (uri[drive] >= L'a' && uri[drive] <= L'z')) &&
           uri[drive + 1] == L':' && uri[drive + 2] == L'/';
}
#endif
inline bool isLocalFileUri(const std::string& uri) {
    return uri.rfind("file:///", 0) == 0 && uri.size() > 8;
}

class WebView {
    using jscb = std::function<void(WebView&, String&)>;

public:
    WebView(int width_ = 800, int height_ = 600, bool resizable_ = true,
            bool debug_ = true, const String& title_ = Str("Webview"),
            const String& url_ = DEFAULT_URL)
        : width(width_),
          height(height_),
          resizable(resizable_),
          debug(debug_),
          title(title_),
          url(url_) {}
    ~WebView() {
        init_done = false;
#if defined(WEBVIEW_EDGE)
        webviewAlive->store(false);
        if (webviewController) webviewController->Close();
        webviewWindow.reset();
        webviewController.reset();
        if (webviewComInitialized) {
            webviewComInitialized = false;
            CoUninitialize();
        }
#elif defined(WEBVIEW_WIN)
        webview = nullptr;
        if (webviewApartmentInitialized) {
            webviewApartmentInitialized = false;
            winrt::uninit_apartment();
        }
#endif
#if defined(WEBVIEW_MSHTML)
        if (hwnd && IsWindow(hwnd)) KillTimer(hwnd, 1);
        if (mshtmlConnectionPoint && mshtmlEventCookie)
            mshtmlConnectionPoint->Unadvise(mshtmlEventCookie);
        mshtmlConnectionPoint.Release();
        mshtmlEventSink.Release();
        mshtmlBrowser.Release();
        if (mshtmlHost && IsWindow(mshtmlHost)) DestroyWindow(mshtmlHost);
        if (mshtmlOleInitialized) {
            mshtmlOleInitialized = false;
            OleUninitialize();
        }
#endif
#if defined(WEBVIEW_IS_WIN)
        if (hwnd && IsWindow(hwnd)) DestroyWindow(hwnd);
#endif
    }
    int init();                            // Initialize webview
    void setCallback(jscb callback);       // JS callback
    void setTitle(String t);               // Set title of window
    void setFullscreen(bool fs);           // Set fullscreen
    void setFullscreenFromJS(bool allow);  // Allow setting fullscreen from JS
    void setBgColor(uint8_t r, uint8_t g, uint8_t b,
                    uint8_t a);      // Set background color
    void setLocalFileOnly(bool enabled) { localFileOnly = enabled; }
    bool run();                      // Main loop
    void navigate(String u);         // Navigate to URL
    void preEval(const String& js);  // Eval JS before page loads
    void eval(const String& js);     // Eval JS
    void css(const String& css);     // Inject CSS
    void exit();                     // Stop loop

private:
    void showForbiddenPage();

    // Properties for init
    int width;
    int height;
    bool resizable;
    bool fullscreen = false;
    bool fullscreenFromJS = false;
    bool localFileOnly = false;
    bool localNotFoundPage = false;
    bool debug;
    String title;
    String url;

    jscb js_callback;
    bool init_done = false;  // Finished running init
    uint8_t bgR = 255, bgG = 255, bgB = 255, bgA = 255;

// Common Windows stuff
#if defined(WEBVIEW_IS_WIN)
    HWND hwnd = nullptr;
    MSG msg{};  // Message from main loop
    bool isFullscreen = false;
    UINT dpi = USER_DEFAULT_SCREEN_DPI;

    struct WindowInfo {
        LONG_PTR style;    // GWL_STYLE
        LONG_PTR exstyle;  // GWL_EXSTYLE
        RECT rect;         // GetWindowRect
    } savedWindowInfo{};

    int WinInit();
#endif
    void prepareLocalStartPage() {
        if (!localFileOnly) return;
#if defined(WEBVIEW_IS_WIN)
        if (isLocalFileUri(url.c_str())) return;
#else
        if (isLocalFileUri(url)) return;
#endif
        std::error_code ec;
        const std::filesystem::path page =
            std::filesystem::current_path(ec) / "index.html";
        if (ec) {
            localNotFoundPage = true;
            url = Str("data:text/html,%3Ctitle%3ENot%20Found%3C/title%3E%3Ch1%3ENot%20Found%3C/h1%3E");
            return;
        }
        if (std::filesystem::is_regular_file(page, ec) && !ec) {
#if defined(WEBVIEW_IS_WIN)
            std::wstring uri(32768, L'\0');
            DWORD length = static_cast<DWORD>(uri.size());
            if (SUCCEEDED(UrlCreateFromPathW(page.c_str(), uri.data(), &length, 0))) {
                uri.resize(wcslen(uri.c_str()));
                url = std::move(uri);
                return;
            }
#else
            const auto path = std::filesystem::absolute(page, ec).generic_string();
            if (!ec) {
                static constexpr char hex[] = "0123456789ABCDEF";
                std::string uri = "file://";
                for (unsigned char ch : path) {
                    if ((ch >= 'a' && ch <= 'z') ||
                        (ch >= 'A' && ch <= 'Z') ||
                        (ch >= '0' && ch <= '9') || ch == '/' || ch == ':' ||
                        ch == '-' || ch == '_' || ch == '.' || ch == '~') {
                        uri += static_cast<char>(ch);
                    } else {
                        uri += '%';
                        uri += hex[ch >> 4];
                        uri += hex[ch & 15];
                    }
                }
                url = uri;
                return;
            }
#endif
        }
        localNotFoundPage = true;
        url = Str("data:text/html,%3Ctitle%3ENot%20Found%3C/title%3E%3Ch1%3ENot%20Found%3C/h1%3E");
    }
#if defined(WEBVIEW_IS_WIN)
    bool isAllowedLocalDocument(const wchar_t* uri) const {
        return isLocalFileUri(uri) ||
               (uri && _wcsicmp(uri, FORBIDDEN_PAGE_URI) == 0) ||
               (uri && _wcsicmp(uri, L"about:blank") == 0) ||
               (localNotFoundPage && uri &&
                _wcsicmp(uri, L"data:text/html,%3Ctitle%3ENot%20Found%3C/title%3E%3Ch1%3ENot%20Found%3C/h1%3E") == 0);
    }
#else
    bool isAllowedLocalDocument(const std::string& uri) const {
        return isLocalFileUri(uri) ||
               uri == "about:blank" ||
               uri == "data:text/html,%3Ctitle%3EForbidden%3C/title%3E%3Ch1%3EForbidden%3C/h1%3E" ||
               (localNotFoundPage &&
                uri == "data:text/html,%3Ctitle%3ENot%20Found%3C/title%3E%3Ch1%3ENot%20Found%3C/h1%3E");
    }
#endif
#if defined(WEBVIEW_IS_WIN)
    void onDPIChange(const UINT dpi, const RECT& rect);
    void resize();
    static LRESULT CALLBACK WndProcedure(HWND hwnd, UINT msg, WPARAM wparam,
                                         LPARAM lparam);
#endif  // WEBVIEW_WIN || WEBVIEW_EDGE

#if defined(WEBVIEW_WIN)
    String inject =
        Str("window.external.invoke=arg=>window.external.notify(arg);");
    WebViewControl webview{nullptr};
    bool webviewApartmentInitialized = false;
    bool initializeScriptRegistered = false;
#elif defined(WEBVIEW_EDGE)  // WEBVIEW_WIN
    String inject = Str(
        "window.external.invoke=arg=>window.chrome.webview.postMessage(arg);");
    wil::com_ptr<ICoreWebView2Controller>
        webviewController;                      // Pointer to WebViewController
    wil::com_ptr<ICoreWebView2> webviewWindow;  // Pointer to WebView window
    bool webviewComInitialized = false;
    bool firstNavigationCompleted = false;
    std::shared_ptr<std::atomic_bool> webviewAlive =
        std::make_shared<std::atomic_bool>(true);
    std::wstring pendingEval;
#elif defined(WEBVIEW_MSHTML)
    String inject = Str(
        "if(!window.__webviewInjected){window.external={invoke:function(arg){document.title='__webview:'+encodeURIComponent(arg)}};window.__webviewInjected=true;}");
    HWND mshtmlHost = nullptr;
    CComPtr<IWebBrowser2> mshtmlBrowser;
    CComPtr<IConnectionPoint> mshtmlConnectionPoint;
    CComPtr<IDispatch> mshtmlEventSink;
    DWORD mshtmlEventCookie = 0;
    bool mshtmlOleInitialized = false;
    String pendingEval;
#elif defined(WEBVIEW_MAC)   // WEBVIEW_EDGE
    String inject =
        Str("window.external={invoke:arg=>window.webkit."
            "messageHandlers.webview.postMessage(arg)};");
    bool should_exit = false;  // Close window
    NSAutoreleasePool* pool;
    NSWindow* window;
    WKWebView* webview;
#elif defined(WEBVIEW_GTK)   // WEBVIEW_MAC
    String inject =
        Str("window.external={invoke:arg=>window.webkit."
            "messageHandlers.external.postMessage(arg)};");
    bool ready = false;        // Done loading page
    bool js_busy = false;      // Currently in JS eval
    bool should_exit = false;  // Close window
    GtkWidget* window;
    GtkWidget* webview;

    static void external_message_received_cb(WebKitUserContentManager* m,
                                             WebKitJavascriptResult* r,
                                             gpointer arg);
    static void webview_eval_finished(GObject* object, GAsyncResult* result,
                                      gpointer arg);
    static void webview_load_changed_cb(WebKitWebView* webview,
                                        WebKitLoadEvent event, gpointer arg);
    static void destroyWindowCb(GtkWidget* widget, gpointer arg);
    // static gboolean closeWebViewCb(WebKitWebView *webView, GtkWidget
    // *window);
    static gboolean webview_context_menu_cb(
        WebKitWebView* webview, GtkWidget* default_menu,
        WebKitHitTestResult* hit_test_result, gboolean triggered_with_keyboard,
        gpointer userdata);
    static gboolean webview_enter_fullscreen_cb(WebKitWebView* webview,
                                                gpointer userdata);
    static gboolean webview_leave_fullscreen_cb(WebKitWebView* webview,
                                                gpointer userdata);
#endif                       // WEBVIEW_GTK
};

// Common Windows methods
#if defined(WEBVIEW_IS_WIN)
auto LoadLibraryPtr(LPCWSTR dll) {
    // WIL alternative: wil::unique_hmodule(LoadLibrary(dll));
    return std::unique_ptr<std::remove_pointer_t<HMODULE>,
                           decltype(&::FreeLibrary)>(LoadLibrary(dll),
                                                     FreeLibrary);
}

void setDPIAwareness() {
    // Set default DPI awareness
    auto user32 = LoadLibraryPtr(TEXT("User32.dll"));
    auto pSPDAC =
        GetProcNameAddress(user32.get(), SetProcessDpiAwarenessContext);
    if (pSPDAC != nullptr) {
        // Windows 10
        pSPDAC(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        return;
    }

    auto shcore = LoadLibraryPtr(TEXT("ShCore.dll"));
    auto pSPDA = GetProcNameAddress(shcore.get(), SetProcessDpiAwareness);
    if (pSPDA != nullptr) {
        // Windows 8.1
        pSPDA(PROCESS_PER_MONITOR_DPI_AWARE);
        return;
    }

    // Windows Vista
    SetProcessDPIAware();  // Equivalent to DPI_AWARENESS_CONTEXT_SYSTEM_AWARE
}

UINT getDPI(HWND hwnd) {
    auto user32 = LoadLibraryPtr(TEXT("User32.dll"));
    auto pGDFW = GetProcNameAddress(user32.get(), GetDpiForWindow);
    if (pGDFW != nullptr) {
        // Windows 10
        return pGDFW(hwnd);
    }

    auto shcore = LoadLibraryPtr(TEXT("ShCore.dll"));
    auto pGDFM = GetProcNameAddress(shcore.get(), GetDpiForMonitor);
    if (pGDFM != nullptr) {
        // Windows 8.1
        UINT newDpi;
        HMONITOR hmonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        HRESULT hr = pGDFM(hmonitor, MDT_EFFECTIVE_DPI, &newDpi, nullptr);
        if (SUCCEEDED(hr)) {
            return newDpi;
        }
    }

    // Windows 2000
    if (HDC hdc = GetDC(hwnd)) {
        auto dpi = GetDeviceCaps(hdc, LOGPIXELSX);
        ReleaseDC(hwnd, hdc);
        return dpi;
    }

    return USER_DEFAULT_SCREEN_DPI;
}

int WebView::WinInit() {
    HINSTANCE hInt = GetModuleHandle(nullptr);
    if (hInt == nullptr) {
        return -1;
    }

    // Initialize Win32 window
    WNDCLASSEX wc{};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = 0;
    wc.lpfnWndProc = WndProcedure;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInt;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = L"webview";
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    if (!RegisterClassEx(&wc)) {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            MessageBox(nullptr, L"Call to RegisterClassEx failed!", L"Error!",
                       MB_OK | MB_ICONERROR);
            return -1;
        }
        WNDCLASSEX existing{};
        existing.cbSize = sizeof(existing);
        if (!GetClassInfoEx(hInt, wc.lpszClassName, &existing) ||
            existing.lpfnWndProc != WndProcedure) {
            MessageBox(nullptr, L"The WebView window class is already in use.",
                       L"Error", MB_OK | MB_ICONERROR);
            return -1;
        }
    }

    // Set default DPI awareness
    setDPIAwareness();

    hwnd = CreateWindow(L"webview", title.c_str(), WS_OVERLAPPEDWINDOW,
                        CW_USEDEFAULT, CW_USEDEFAULT, width, height, nullptr,
                        nullptr, hInt, nullptr);

    if (hwnd == nullptr) {
        MessageBox(nullptr, L"Window Registration Failed!", L"Error!",
                   MB_ICONEXCLAMATION | MB_OK);
        return -1;
    }

    // Scale window based on DPI
    dpi = getDPI(hwnd);

    RECT rect = {};
    GetWindowRect(hwnd, &rect);
    SetWindowPos(hwnd, nullptr, rect.left, rect.top,
                 MulDiv(width, dpi, USER_DEFAULT_SCREEN_DPI),
                 MulDiv(height, dpi, USER_DEFAULT_SCREEN_DPI),
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    if (!resizable) {
        auto style = GetWindowLongPtr(hwnd, GWL_STYLE);
        style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
        SetWindowLongPtr(hwnd, GWL_STYLE, style);
    }

    // Used with GetWindowLongPtr in WndProcedure
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)this);
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);
    SetFocus(hwnd);

    return 0;
}

void WebView::setTitle(std::wstring t) {
    if (!init_done) {
        title = t;
    } else {
        SetWindowText(hwnd, t.c_str());
    }
}

// Adapted from
// https://source.chromium.org/chromium/chromium/src/+/main:ui/views/win/fullscreen_handler.cc
void WebView::setFullscreen(bool fs) {
    if (isFullscreen == fs) return;

    isFullscreen = fs;

    if (fs) {
        // Store window style before going fullscreen
        savedWindowInfo.style = GetWindowLongPtr(hwnd, GWL_STYLE);
        savedWindowInfo.exstyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
        GetWindowRect(hwnd, &savedWindowInfo.rect);

        // Set new window style
        SetWindowLongPtr(hwnd, GWL_STYLE,
                         savedWindowInfo.style & ~(WS_CAPTION | WS_THICKFRAME));
        SetWindowLongPtr(
            hwnd, GWL_EXSTYLE,
            savedWindowInfo.exstyle & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE |
                                        WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));

        // Get monitor size
        MONITORINFO monitorInfo;
        monitorInfo.cbSize = sizeof(monitorInfo);
        GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST),
                       &monitorInfo);

        // Set window size to monitor size
        SetWindowPos(hwnd, nullptr, monitorInfo.rcMonitor.left,
                     monitorInfo.rcMonitor.top,
                     monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                     monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                     SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    } else {
        // Restore window style
        SetWindowLongPtr(hwnd, GWL_STYLE, savedWindowInfo.style);
        SetWindowLongPtr(hwnd, GWL_EXSTYLE, savedWindowInfo.exstyle);

        // Restore window size
        SetWindowPos(hwnd, nullptr, savedWindowInfo.rect.left,
                     savedWindowInfo.rect.top,
                     savedWindowInfo.rect.right - savedWindowInfo.rect.left,
                     savedWindowInfo.rect.bottom - savedWindowInfo.rect.top,
                     SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
}

bool WebView::run() {
    bool loop = GetMessage(&msg, nullptr, 0, 0) > 0;
    if (loop) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return !loop;
}

LRESULT CALLBACK WebView::WndProcedure(HWND hwnd, UINT msg, WPARAM wparam,
                                       LPARAM lparam) {
    WebView* w =
        reinterpret_cast<WebView*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (msg) {
        case WM_SIZE:
            // WM_SIZE will first fire before the webview finishes loading
            // init() calls resize(), so this call is only for size changes
            // after fully loading.
            if (w != nullptr && w->init_done) {
                w->resize();
            }
            return DefWindowProc(hwnd, msg, wparam, lparam);
#if defined(WEBVIEW_EDGE) || defined(WEBVIEW_MSHTML)
        case WM_APP + 42:
            if (w != nullptr) w->showForbiddenPage();
            return 0;
#endif
#if defined(WEBVIEW_EDGE)
        case WM_MOVE:
        case WM_MOVING:
            if (w != nullptr && w->webviewController) {
                w->webviewController->NotifyParentWindowPositionChanged();
            }
            return DefWindowProc(hwnd, msg, wparam, lparam);
#endif
#if defined(WEBVIEW_MSHTML)
        case WM_TIMER:
            if (w != nullptr && w->init_done && w->mshtmlBrowser) {
                CComPtr<IDispatch> disp;
                CComPtr<IHTMLDocument2> doc;
                if (SUCCEEDED(w->mshtmlBrowser->get_Document(&disp)) && disp &&
                    SUCCEEDED(disp->QueryInterface(IID_PPV_ARGS(&doc))) && doc) {
                    CComPtr<IHTMLWindow2> win;
                    if (SUCCEEDED(doc->get_parentWindow(&win)) && win) {
                        CComBSTR ready;
                        doc->get_readyState(&ready);
                        if (ready && wcscmp(ready, L"complete") == 0) {
                            CComBSTR script(w->inject.c_str());
                            CComBSTR language(L"javascript");
                            win->execScript(script, language, nullptr);
                            if (!w->pendingEval.empty()) {
                                CComBSTR queued(w->pendingEval.c_str());
                                win->execScript(queued, language, nullptr);
                                w->pendingEval.clear();
                            }
                        }
                    }
                    CComBSTR title;
                    if (SUCCEEDED(doc->get_title(&title)) && title &&
                        wcsncmp(title, L"__webview:", 10) == 0) {
                        std::wstring encoded(static_cast<BSTR>(title) + 10);
                        std::string utf8;
                        for (size_t i = 0; i < encoded.size();) {
                            if (encoded[i] == L'%' && i + 2 < encoded.size()) {
                                wchar_t hex[3] = {encoded[i + 1], encoded[i + 2], 0};
                                utf8.push_back(static_cast<char>(
                                    wcstol(hex, nullptr, 16)));
                                i += 3;
                            } else {
                                utf8.push_back(static_cast<char>(encoded[i++]));
                            }
                        }
                        int chars = MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                                                        static_cast<int>(utf8.size()),
                                                        nullptr, 0);
                        std::wstring message(chars > 0 ? chars : 0, L'\0');
                        if (chars > 0) {
                            MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                                                static_cast<int>(utf8.size()),
                                                message.data(), chars);
                        }
                        if (w->js_callback) w->js_callback(*w, message);
                        doc->put_title(CComBSTR(L""));
                    }
                }
            }
            return 0;
#endif
        case WM_DPICHANGED:
            if (w != nullptr) {
                // Resize on DPI change
                UINT dpi = HIWORD(wparam);
                const auto& rect = *reinterpret_cast<RECT*>(lparam);
                w->onDPIChange(dpi, rect);
            }
            break;
        case WM_DESTROY:
            w->exit();
            break;
        default:
            return DefWindowProc(hwnd, msg, wparam, lparam);
    }
    return 0;
}

void WebView::onDPIChange(const UINT newDpi, const RECT& rect) {
    auto oldDpi = std::exchange(dpi, newDpi);

    if (isFullscreen) {
        // Scale the saved window size by the change in DPI
        auto oldWidth = savedWindowInfo.rect.right - savedWindowInfo.rect.left;
        auto oldHeight = savedWindowInfo.rect.bottom - savedWindowInfo.rect.top;
        savedWindowInfo.rect.right =
            savedWindowInfo.rect.left + MulDiv(oldWidth, newDpi, oldDpi);
        savedWindowInfo.rect.bottom =
            savedWindowInfo.rect.top + MulDiv(oldHeight, newDpi, oldDpi);
    } else {
        SetWindowPos(hwnd, nullptr, rect.left, rect.top, rect.right - rect.left,
                     rect.bottom - rect.top,
                     SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
}
#endif

void WebView::showForbiddenPage() {
#if defined(WEBVIEW_WIN)
    webview.NavigateToString(L"<title>Forbidden</title><h1>Forbidden</h1>");
#elif defined(WEBVIEW_EDGE)
    if (webviewWindow) webviewWindow->Navigate(FORBIDDEN_PAGE_URI);
#elif defined(WEBVIEW_MSHTML)
    CComPtr<IDispatch> dispatch;
    CComPtr<IHTMLDocument2> document;
    if (SUCCEEDED(mshtmlBrowser->get_Document(&dispatch)) && dispatch &&
        SUCCEEDED(dispatch->QueryInterface(IID_PPV_ARGS(&document))) && document) {
        SAFEARRAY* html = SafeArrayCreateVector(VT_VARIANT, 0, 1);
        if (!html) return;
        LONG index = 0;
        CComVariant markup(L"<title>Forbidden</title><h1>Forbidden</h1>");
        if (SUCCEEDED(SafeArrayPutElement(html, &index, &markup)))
            document->write(html);
        SafeArrayDestroy(html);
        document->close();
    }
#elif defined(WEBVIEW_MAC)
    [webview loadHTMLString:@"<title>Forbidden</title><h1>Forbidden</h1>"
                    baseURL:nil];
#elif defined(WEBVIEW_GTK)
    webkit_web_view_load_html(WEBKIT_WEB_VIEW(webview),
                              "<title>Forbidden</title><h1>Forbidden</h1>",
                              nullptr);
#endif
}

#if defined(WEBVIEW_WIN)
// Await helper
template <typename T>
auto block(T const& async) {
    if (async.Status() != AsyncStatus::Completed) {
        winrt::handle h(CreateEvent(nullptr, false, false, nullptr));
        async.Completed([h = h.get()](auto, auto) { SetEvent(h); });
        HANDLE hs[] = {h.get()};
        DWORD i;
        CoWaitForMultipleHandles(COWAIT_DISPATCH_WINDOW_MESSAGES |
                                     COWAIT_DISPATCH_CALLS |
                                     COWAIT_INPUTAVAILABLE,
                                 INFINITE, 1, hs, &i);
    }
    return async.GetResults();
}

int WebView::init() {
    if (auto res = WinInit(); res) {
        return res;
    }

    try {
    // Set to single-thread
    init_apartment(winrt::apartment_type::single_threaded);
    webviewApartmentInitialized = true;

    // Allow intranet access (and localhost)
    WebViewControlProcessOptions options;
    options.PrivateNetworkClientServerCapability(
        WebViewControlProcessCapabilityState::Enabled);

    WebViewControlProcess proc(options);
    webview = block(proc.CreateWebViewControlAsync(
        reinterpret_cast<int64_t>(hwnd), Rect()));
    webview.Settings().IsScriptNotifyAllowed(true);
    webview.ScriptNotify([this](const auto&, const auto& args) {
        if (js_callback) {
            std::wstring ws{args.Value()};
            js_callback(*this, ws);
        }
    });
    webview.NavigationStarting([this](const auto&, const auto& args) {
        if (localFileOnly &&
            !isAllowedLocalDocument(args.Uri().AbsoluteUri().c_str())) {
            args.Cancel(true);
            showForbiddenPage();
            return;
        }
        if (!initializeScriptRegistered) {
            webview.AddInitializeScript(inject);
            initializeScriptRegistered = true;
        }
    });

    // Detect fullscreen request from JS
    webview.ContainsFullScreenElementChanged([this](const auto&, const auto&) {
        if (fullscreenFromJS) {
            this->setFullscreen(webview.ContainsFullScreenElement());
        }
    });

    // Set webview bounds
    resize();

    webview.IsVisible(true);

    // Done initialization, set properties
    init_done = true;

    setTitle(title);
    if (fullscreen) {
        setFullscreen(true);
    }
    setBgColor(bgR, bgG, bgB, bgA);
    prepareLocalStartPage();
    navigate(url);

    return 0;
    } catch (const winrt::hresult_error&) {
        MessageBox(hwnd, L"Could not initialize the EdgeHTML WebView.",
                   L"WebView error", MB_ICONERROR | MB_OK);
        return -1;
    }
}

void WebView::setFullscreenFromJS(bool allow) { fullscreenFromJS = allow; }

void WebView::setBgColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!init_done) {
        bgR = r;
        bgG = g;
        bgB = b;
        bgA = a;
    } else {
        webview.DefaultBackgroundColor({a, r, g, b});
    }
}

void WebView::navigate(std::wstring u) {
    if (!init_done) {
        url = u;
    } else if (localFileOnly && !isAllowedLocalDocument(u.c_str())) {
        showForbiddenPage();
        return;
    } else if (localNotFoundPage && u == url) {
        webview.NavigateToString(L"<title>Not Found</title><h1>Not Found</h1>");
    } else if (constexpr auto prefix = L"data:text/html,";
               u.rfind(prefix, 0) == 0) {
        constexpr auto len = std::wstring_view(prefix).size();
        webview.NavigateToString(u.substr(len));
    } else {
        Uri uri{u};
        webview.Navigate(uri);
    }
}

void WebView::eval(const std::wstring& js) {
    auto result = block(webview.InvokeScriptAsync(
        L"eval", std::vector<winrt::hstring>({winrt::hstring(js)})));

    // if (debug) {
    // std::cout << winrt::to_string(result) << std::endl;
    //}
}

void WebView::exit() { PostQuitMessage(WM_QUIT); }

void WebView::resize() {
    RECT rc;
    GetClientRect(hwnd, &rc);
    Rect bounds((float)rc.left, (float)rc.top, (float)(rc.right - rc.left),
                (float)(rc.bottom - rc.top));
    webview.Bounds(bounds);
}
#elif defined(WEBVIEW_EDGE)  // WEBVIEW_WIN
int WebView::init() {
    if (auto res = WinInit(); res) {
        return res;
    }

    // Set to single-thread
    auto inithr = CoInitializeEx(
        nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(inithr)) {
        return -1;
    }
    webviewComInitialized = true;

    const std::weak_ptr<std::atomic_bool> lifetime = webviewAlive;
    auto onWebMessageReceieved =
        [this, lifetime](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) {
            const auto alive = lifetime.lock();
            if (!alive || !alive->load()) return S_OK;
            if (js_callback) {
                // Consider args->get_WebMessageAsJson?
                LPWSTR messageRaw;
                auto getMessageResult =
                    args->TryGetWebMessageAsString(&messageRaw);
                if (FAILED(getMessageResult)) {
                    return getMessageResult;
                }

                std::wstring message(messageRaw);
                CoTaskMemFree(messageRaw);
                js_callback(*this, message);
            }
            return S_OK;
        };

    auto onWebViewControllerCreate =
        [this, onWebMessageReceieved, lifetime](
            HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
        const auto alive = lifetime.lock();
        if (!alive || !alive->load()) {
            if (controller) controller->Close();
            return S_OK;
        }
        if (FAILED(result) || controller == nullptr) {
            MessageBox(hwnd, L"Could not create the WebView2 controller.",
                       L"WebView2 error", MB_ICONERROR | MB_OK);
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            return FAILED(result) ? result : E_FAIL;
        }

        webviewController = controller;
        HRESULT hr = webviewController->get_CoreWebView2(&webviewWindow);
        if (FAILED(hr) || !webviewWindow) {
            MessageBox(hwnd, L"Could not initialize the WebView2 control.",
                       L"WebView2 error", MB_ICONERROR | MB_OK);
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            return FAILED(hr) ? hr : E_FAIL;
        }

        wil::com_ptr<ICoreWebView2Settings> settings;
        hr = webviewWindow->get_Settings(&settings);
        if (FAILED(hr) || !settings) {
            MessageBox(hwnd, L"Could not read WebView2 settings.",
                       L"WebView2 error", MB_ICONERROR | MB_OK);
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            return FAILED(hr) ? hr : E_FAIL;
        }
        if (!debug && FAILED(settings->put_AreDevToolsEnabled(FALSE))) {
            MessageBox(hwnd, L"Could not configure WebView2 settings.",
                       L"WebView2 error", MB_ICONERROR | MB_OK);
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            return E_FAIL;
        }

        // Resize WebView
        resize();

        hr = webviewWindow->add_WebMessageReceived(
            Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                onWebMessageReceieved)
                .Get(),
            nullptr);
        if (FAILED(hr)) {
            MessageBox(hwnd, L"Could not connect WebView2 messages.",
                       L"WebView2 error", MB_ICONERROR | MB_OK);
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            return hr;
        }

        hr = webviewWindow->add_NavigationStarting(
            Callback<ICoreWebView2NavigationStartingEventHandler>(
                [this](ICoreWebView2*,
                       ICoreWebView2NavigationStartingEventArgs* args) {
                    if (!localFileOnly || !args) return S_OK;
                    LPWSTR uri = nullptr;
                    const HRESULT uriResult = args->get_Uri(&uri);
                    const bool allowed = SUCCEEDED(uriResult) &&
                                         isAllowedLocalDocument(uri);
                    CoTaskMemFree(uri);
                    if (allowed) return S_OK;
                    const HRESULT cancelResult = args->put_Cancel(TRUE);
                    PostMessage(hwnd, WM_APP + 42, 0, 0);
                    return cancelResult;
                })
                .Get(),
            nullptr);
        if (FAILED(hr)) {
            MessageBox(hwnd, L"Could not connect WebView2 navigation events.",
                       L"WebView2 error", MB_ICONERROR | MB_OK);
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            return hr;
        }

        hr = webviewWindow->add_NewWindowRequested(
            Callback<ICoreWebView2NewWindowRequestedEventHandler>(
                [this](ICoreWebView2*,
                       ICoreWebView2NewWindowRequestedEventArgs* args) {
                    if (!localFileOnly || !args) return S_OK;
                    LPWSTR uri = nullptr;
                    const HRESULT uriResult = args->get_Uri(&uri);
                    HRESULT hr = args->put_Handled(TRUE);
                    if (SUCCEEDED(hr)) {
                        if (SUCCEEDED(uriResult) &&
                            isAllowedLocalDocument(uri)) {
                            hr = webviewWindow->Navigate(uri);
                        } else {
                            PostMessage(hwnd, WM_APP + 42, 0, 0);
                        }
                    }
                    CoTaskMemFree(uri);
                    return hr;
                })
                .Get(),
            nullptr);
        if (FAILED(hr)) {
            MessageBox(hwnd, L"Could not connect WebView2 window events.",
                       L"WebView2 error", MB_ICONERROR | MB_OK);
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            return hr;
        }

        // Detect fullscreen change from JS
        hr = webviewWindow->add_ContainsFullScreenElementChanged(
            Callback<ICoreWebView2ContainsFullScreenElementChangedEventHandler>(
                [this](ICoreWebView2*, IUnknown*) {
                    if (fullscreenFromJS) {
                        BOOL containsFs = false;
                        webviewWindow->get_ContainsFullScreenElement(
                            &containsFs);
                        this->setFullscreen(containsFs);
                    }

                    return S_OK;
                })
                .Get(),
            nullptr);
        if (FAILED(hr)) {
            MessageBox(hwnd, L"Could not connect WebView2 events.",
                       L"WebView2 error", MB_ICONERROR | MB_OK);
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            return hr;
        }

        hr = webviewWindow->add_NavigationCompleted(
            Callback<ICoreWebView2NavigationCompletedEventHandler>(
                [this](ICoreWebView2*,
                       ICoreWebView2NavigationCompletedEventArgs* args) {
                    BOOL succeeded = FALSE;
                    if (args && SUCCEEDED(args->get_IsSuccess(&succeeded)) &&
                        succeeded) {
                        firstNavigationCompleted = true;
                        if (!pendingEval.empty()) {
                            auto scripts = std::move(pendingEval);
                            pendingEval.clear();
                            eval(scripts);
                        }
                    }
                    return S_OK;
                })
                .Get(),
            nullptr);
        if (FAILED(hr)) {
            MessageBox(hwnd, L"Could not connect WebView2 navigation events.",
                       L"WebView2 error", MB_ICONERROR | MB_OK);
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            return hr;
        }

        // Document-start script registration is asynchronous. Navigate only
        // after the completion callback confirms that it is ready.
        hr = webviewWindow->AddScriptToExecuteOnDocumentCreated(
            inject.c_str(),
            Callback<ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler>(
                [this, lifetime](HRESULT scriptResult, LPCWSTR) -> HRESULT {
                    const auto alive = lifetime.lock();
                    if (!alive || !alive->load()) return S_OK;
                    if (FAILED(scriptResult)) {
                        MessageBox(hwnd,
                                   L"Could not register the WebView2 startup script.",
                                   L"WebView2 error", MB_ICONERROR | MB_OK);
                        PostMessage(hwnd, WM_CLOSE, 0, 0);
                        return scriptResult;
                    }

                    init_done = true;
                    setTitle(title);
                    if (fullscreen) setFullscreen(true);
                    setBgColor(bgR, bgG, bgB, bgA);
                    prepareLocalStartPage();
                    navigate(url);
                    return S_OK;
                })
                .Get());
        if (FAILED(hr)) {
            MessageBox(hwnd, L"Could not register the WebView2 startup script.",
                       L"WebView2 error", MB_ICONERROR | MB_OK);
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            return hr;
        }

        return S_OK;
    };

    auto onCreateEnvironment = [this, onWebViewControllerCreate, lifetime](
                                   HRESULT result,
                                   ICoreWebView2Environment* env) -> HRESULT {
        const auto alive = lifetime.lock();
        if (!alive || !alive->load()) return S_OK;
        if (FAILED(result) || env == nullptr) {
            const wchar_t* message =
                result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)
                    ? L"Could not find the WebView2 Runtime."
                    : L"Could not create the WebView2 environment.";
            MessageBox(hwnd, message, L"WebView2 error",
                       MB_ICONERROR | MB_OK);
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            return FAILED(result) ? result : E_FAIL;
        }

        HRESULT hr = env->CreateCoreWebView2Controller(
            hwnd,
            Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                onWebViewControllerCreate)
                .Get());
        if (FAILED(hr)) {
            MessageBox(hwnd, L"Could not start WebView2 controller creation.",
                       L"WebView2 error", MB_ICONERROR | MB_OK);
            PostMessage(hwnd, WM_CLOSE, 0, 0);
        }
        return hr;
    };

    // Use a writable, machine-local profile directory. APPDATA can be
    // redirected to a network or roaming profile.
    std::wstring userDataFolder;
    DWORD bufferLength = GetEnvironmentVariable(
        Str("LOCALAPPDATA"), nullptr, 0);
    if (bufferLength > 1) {
        std::wstring localAppData(bufferLength, L'\0');
        DWORD written = GetEnvironmentVariable(
            Str("LOCALAPPDATA"), localAppData.data(), bufferLength);
        if (written > 0 && written < bufferLength) {
            localAppData.resize(written);
            wchar_t exePath[32768]{};
            DWORD exePathLength = GetModuleFileName(
                nullptr, exePath, static_cast<DWORD>(std::size(exePath)));
            if (exePathLength > 0 &&
                exePathLength < static_cast<DWORD>(std::size(exePath))) {
                userDataFolder = localAppData + Str("\\") +
                                 PathFindFileName(exePath) +
                                 Str(".WebView2");
            }
        }
    }

    if (userDataFolder.empty()) {
        MessageBox(hwnd,
                   L"Could not determine a writable WebView2 profile folder.",
                   L"WebView2 error", MB_ICONERROR | MB_OK);
        webviewComInitialized = false;
        CoUninitialize();
        return -1;
    }

    auto hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userDataFolder.c_str(), nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            onCreateEnvironment)
            .Get());

    if (FAILED(hr)) {
        webviewComInitialized = false;
        CoUninitialize();
        return -1;
    }

    return 0;
}

void WebView::setFullscreenFromJS(bool allow) { fullscreenFromJS = allow; }

void WebView::setBgColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!init_done) {
        bgR = r;
        bgG = g;
        bgB = b;
        bgA = a;
        return;
    }

    wil::com_ptr<ICoreWebView2Controller2> controller2;
    if (SUCCEEDED(
            webviewController->QueryInterface(IID_PPV_ARGS(&controller2))) &&
        controller2) {
        COREWEBVIEW2_COLOR color{static_cast<BYTE>(a == 0 ? 0 : 255), r, g, b};
        controller2->put_DefaultBackgroundColor(color);
    }
}

void WebView::navigate(std::wstring u) {
    if (!init_done) {
        url = u;
    } else if (localFileOnly && !isAllowedLocalDocument(u.c_str())) {
        showForbiddenPage();
    } else {
        if (localNotFoundPage && u == url)
            webviewWindow->NavigateToString(
                L"<title>Not Found</title><h1>Not Found</h1>");
        else
            webviewWindow->Navigate(u.c_str());
    }
}

void WebView::eval(const std::wstring& js) {
    if (!firstNavigationCompleted || !webviewWindow) {
        pendingEval += js;
        pendingEval += L"\n";
        return;
    }
    webviewWindow->ExecuteScript(
        js.c_str(), Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
                        [](HRESULT, LPCWSTR) -> HRESULT { return S_OK; })
                        .Get());
}

void WebView::exit() {
    PostQuitMessage(WM_QUIT);
}

void WebView::resize() {
    RECT rc;
    GetClientRect(hwnd, &rc);
    webviewController->put_Bounds(rc);
}
#elif defined(WEBVIEW_MSHTML)
int WebView::init() {
    if (WinInit() != 0) return -1;
    const HRESULT oleResult = OleInitialize(nullptr);
    if (FAILED(oleResult)) return -1;
    mshtmlOleInitialized = true;
    if (!AtlAxWinInit()) return -1;
    RECT rc{};
    GetClientRect(hwnd, &rc);
    mshtmlHost = CreateWindowEx(0, L"AtlAxWin", L"Shell.Explorer.2",
                                WS_CHILD | WS_VISIBLE, 0, 0, rc.right, rc.bottom,
                                hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    CComPtr<IUnknown> control;
    if (!mshtmlHost || FAILED(AtlAxGetControl(mshtmlHost, &control)) ||
        !control || FAILED(control->QueryInterface(IID_PPV_ARGS(&mshtmlBrowser))))
        return -1;
    CComPtr<IConnectionPointContainer> connectionContainer;
    CComObject<MshtmlNavigationSink>* sink = nullptr;
    HRESULT eventHr = mshtmlBrowser->QueryInterface(
        IID_PPV_ARGS(&connectionContainer));
    if (SUCCEEDED(eventHr))
        eventHr = connectionContainer->FindConnectionPoint(
            DIID_DWebBrowserEvents2, &mshtmlConnectionPoint);
    if (SUCCEEDED(eventHr))
        eventHr = CComObject<MshtmlNavigationSink>::CreateInstance(&sink);
    if (SUCCEEDED(eventHr) && sink) {
        sink->AddRef();
        sink->beforeNavigate = [this](BSTR target, VARIANT_BOOL* cancel) {
            if (!localFileOnly || isAllowedLocalDocument(target)) return;
            if (cancel) *cancel = VARIANT_TRUE;
            PostMessage(hwnd, WM_APP + 42, 0, 0);
        };
        CComPtr<IDispatch> eventSink;
        eventHr = sink->QueryInterface(IID_PPV_ARGS(&eventSink));
        if (SUCCEEDED(eventHr)) {
            eventHr = mshtmlConnectionPoint->Advise(
                eventSink, &mshtmlEventCookie);
            if (SUCCEEDED(eventHr)) mshtmlEventSink = eventSink;
        }
        sink->Release();
    }
    if (FAILED(eventHr)) {
        if (mshtmlConnectionPoint && mshtmlEventCookie)
            mshtmlConnectionPoint->Unadvise(mshtmlEventCookie);
        mshtmlConnectionPoint.Release();
        return -1;
    }
    const HRESULT silentResult =
        mshtmlBrowser->put_Silent(debug ? VARIANT_FALSE : VARIANT_TRUE);
    if (FAILED(silentResult)) return -1;
    if (!SetTimer(hwnd, 1, 100, nullptr)) return -1;
    init_done = true;
    setTitle(title);
    if (fullscreen) setFullscreen(true);
    setBgColor(bgR, bgG, bgB, bgA);
    prepareLocalStartPage();
    navigate(url);
    return 0;
}

void WebView::setFullscreenFromJS(bool allow) { fullscreenFromJS = allow; }

void WebView::setBgColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!init_done) {
        bgR = r;
        bgG = g;
        bgB = b;
        bgA = a;
    }
}

void WebView::navigate(std::wstring u) {
    if (!init_done) {
        url = u;
        return;
    }
    if (localFileOnly && !isAllowedLocalDocument(u.c_str())) {
        showForbiddenPage();
        return;
    }
    if (localNotFoundPage && u == url) {
        CComPtr<IDispatch> dispatch;
        CComPtr<IHTMLDocument2> document;
        if (SUCCEEDED(mshtmlBrowser->get_Document(&dispatch)) && dispatch &&
            SUCCEEDED(dispatch->QueryInterface(IID_PPV_ARGS(&document))) &&
            document) {
            SAFEARRAY* html = SafeArrayCreateVector(VT_VARIANT, 0, 1);
            if (!html) return;
            LONG index = 0;
            CComVariant markup(L"<title>Not Found</title><h1>Not Found</h1>");
            if (SUCCEEDED(SafeArrayPutElement(html, &index, &markup)))
                document->write(html);
            SafeArrayDestroy(html);
            document->close();
        }
        return;
    }
    CComVariant address(u.c_str()), empty;
    mshtmlBrowser->Navigate2(&address, &empty, &empty, &empty, &empty);
}

void WebView::eval(const std::wstring& js) {
    if (!init_done) { pendingEval += js + L"\n"; return; }
    CComPtr<IDispatch> disp;
    CComPtr<IHTMLDocument2> doc;
    CComPtr<IHTMLWindow2> win;
    if (SUCCEEDED(mshtmlBrowser->get_Document(&disp)) && disp &&
        SUCCEEDED(disp->QueryInterface(IID_PPV_ARGS(&doc))) && doc &&
        SUCCEEDED(doc->get_parentWindow(&win)) && win) {
        CComBSTR script(js.c_str()), language(L"javascript");
        win->execScript(script, language, nullptr);
    } else {
        pendingEval += js + L"\n";
    }
}

void WebView::exit() { KillTimer(hwnd, 1); PostQuitMessage(WM_QUIT); }

void WebView::resize() {
    RECT rc;
    GetClientRect(hwnd, &rc);
    if (mshtmlHost) MoveWindow(mshtmlHost, 0, 0, rc.right, rc.bottom, TRUE);
}

#elif defined(WEBVIEW_MAC)   // WEBVIEW_EDGE
int WebView::init() {
    // Initialize autorelease pool
    pool = [NSAutoreleasePool new];

    // Window style: titled, closable, minimizable
    uint style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                 NSWindowStyleMaskMiniaturizable;

    // Set window to be resizable
    if (resizable) {
        style |= NSWindowStyleMaskResizable;
    }

    // Initialize Cocoa window
    window = [[NSWindow alloc]
        // Initial window size
        initWithContentRect:NSMakeRect(0, 0, width, height)
                  // Window style
                  styleMask:style
                    backing:NSBackingStoreBuffered
                      defer:NO];

    // Minimum window size
    [window setContentMinSize:NSMakeSize(width, height)];

    // Position window in center of screen
    [window center];

    // Initialize WKWebView
    WKWebViewConfiguration* config = [WKWebViewConfiguration new];
    WKPreferences* prefs = [config preferences];
    [prefs setJavaScriptCanOpenWindowsAutomatically:NO];
    if (debug) {
        [prefs setValue:@YES forKey:@"developerExtrasEnabled"];
    }

    // Allow fullscreen control from JS
    if (fullscreenFromJS) {
        [prefs setValue:@YES forKey:@"fullScreenEnabled"];
    }

    WKUserContentController* controller = [config userContentController];
    // Add inject script
    WKUserScript* userScript = [WKUserScript alloc];
    [userScript initWithSource:[NSString stringWithUTF8String:inject.c_str()]
                 injectionTime:WKUserScriptInjectionTimeAtDocumentStart
              forMainFrameOnly:NO];
    [controller addUserScript:userScript];

    webview = [[WKWebView alloc] initWithFrame:NSZeroRect configuration:config];

    // Add delegate methods manually in order to capture "this"
    class_replaceMethod(
        [WindowDelegate class], @selector(windowWillClose:),
        imp_implementationWithBlock([=](id, SEL, id) { this->exit(); }),
        "v@:@");

    class_replaceMethod(
        [WindowDelegate class],
        @selector(webView:decidePolicyForNavigationAction:decisionHandler:),
        imp_implementationWithBlock(
            [=](id, SEL, WKWebView* view, WKNavigationAction* action,
                void (^decisionHandler)(WKNavigationActionPolicy)) {
                NSURL* target = [[action request] URL];
                BOOL local = [target isFileURL] &&
                             ([[target host] length] == 0 ||
                              [[[target host] lowercaseString]
                                  isEqualToString:@"localhost"]);
                BOOL blank = [[target absoluteString]
                                  isEqualToString:@"about:blank"];
                const BOOL notFound = this->localNotFoundPage &&
                    [[target absoluteString] isEqualToString:
                        @"data:text/html,%3Ctitle%3ENot%20Found%3C/title%3E%3Ch1%3ENot%20Found%3C/h1%3E"];
                const BOOL allowed = !this->localFileOnly || local || blank || notFound;
                decisionHandler(allowed ? WKNavigationActionPolicyAllow
                                        : WKNavigationActionPolicyCancel);
                if (!allowed) {
                    dispatch_async(dispatch_get_main_queue(), ^{
                        [view loadHTMLString:
                                  @"<title>Forbidden</title><h1>Forbidden</h1>"
                                  baseURL:nil];
                    });
                }
            }),
        "v@:@@@");

    class_replaceMethod(
        [WindowDelegate class],
        @selector(userContentController:didReceiveScriptMessage:),
        imp_implementationWithBlock(
            [=](id, SEL, WKScriptMessage* scriptMessage) {
                if (this->js_callback) {
                    id body = [scriptMessage body];
                    if (![body isKindOfClass:[NSString class]]) {
                        return;
                    }

                    std::string msg = [body UTF8String];
                    this->js_callback(*this, msg);
                }
            }),
        "v@:@");

    WindowDelegate* delegate = [WindowDelegate alloc];
    [controller addScriptMessageHandler:delegate name:@"webview"];
    // Set delegate to window
    [window setDelegate:delegate];
    [webview setNavigationDelegate:delegate];

    // Initialize application
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    // Sets the app as the active app
    [NSApp activateIgnoringOtherApps:YES];

    // Add webview to window
    [window setContentView:webview];

    // Display window
    [window makeKeyAndOrderFront:nil];

    // Done initialization, set properties
    init_done = true;

    setTitle(title);
    if (fullscreen) {
        setFullscreen(true);
    }
    setBgColor(bgR, bgG, bgB, bgA);
    prepareLocalStartPage();
    navigate(url);

    return 0;
}

void WebView::setTitle(std::string t) {
    if (!init_done) {
        title = t;
    } else {
        [window setTitle:[NSString stringWithUTF8String:t.c_str()]];
    }
}

void WebView::setFullscreen(bool fs) {
    if (!init_done) {
        fullscreen = fs;
    } else {
        // TODO: replace toggle with set
        [window toggleFullScreen:nil];
    }
}

void WebView::setFullscreenFromJS(bool allow) { fullscreenFromJS = allow; }

void WebView::setBgColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!init_done) {
        bgR = r;
        bgG = g;
        bgB = b;
        bgA = a;
    } else {
        [window setBackgroundColor:[NSColor colorWithCalibratedRed:r / 255.0
                                                             green:g / 255.0
                                                              blue:b / 255.0
                                                             alpha:a / 255.0]];
    }
}

bool WebView::run() {
    NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                        untilDate:[NSDate distantFuture]
                                           inMode:NSDefaultRunLoopMode
                                          dequeue:true];
    if (event) {
        [NSApp sendEvent:event];
    }

    return should_exit;
}

void WebView::navigate(std::string u) {
    if (!init_done) {
        url = u;
    } else if (localFileOnly && !isAllowedLocalDocument(u)) {
        showForbiddenPage();
        return;
    } else if (localNotFoundPage && u == url) {
        [webview loadHTMLString:@"<title>Not Found</title><h1>Not Found</h1>"
                        baseURL:nil];
    } else if (u.rfind("data:", 0) == 0) {
        [webview loadHTMLString:[NSString stringWithUTF8String:u.c_str()]
                        baseURL:nil];
    } else {
        [webview
            loadRequest:[NSURLRequest
                            requestWithURL:
                                [NSURL URLWithString:[NSString
                                                         stringWithUTF8String:
                                                             u.c_str()]]]];
    }
}

void WebView::eval(const std::string& js) {
    [webview evaluateJavaScript:[NSString stringWithUTF8String:js.c_str()]
              completionHandler:nil];
}

void WebView::exit() {
    // Distinguish window closing with app exiting
    should_exit = true;
    [NSApp terminate:nil];
}

#elif defined(WEBVIEW_GTK)  // WEBVIEW_MAC
int WebView::init() {
    if (gtk_init_check(0, NULL) == FALSE) {
        return -1;
    }

    // Initialize GTK window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    if (resizable) {
        gtk_window_set_default_size(GTK_WINDOW(window), width, height);
    } else {
        gtk_widget_set_size_request(window, width, height);
    }

    gtk_window_set_resizable(GTK_WINDOW(window), resizable);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

    // Add scrolling container
    GtkWidget* scroller = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_container_add(GTK_CONTAINER(window), scroller);

    // Content manager
    WebKitUserContentManager* cm = webkit_user_content_manager_new();
    webkit_user_content_manager_register_script_message_handler(cm, "external");
    g_signal_connect(cm, "script-message-received::external",
                     G_CALLBACK(external_message_received_cb), this);

    // WebView
    webview = webkit_web_view_new_with_user_content_manager(cm);
    g_signal_connect(
        G_OBJECT(webview), "decide-policy",
        G_CALLBACK(+[](WebKitWebView* view, WebKitPolicyDecision* decision,
                       WebKitPolicyDecisionType type, gpointer data) -> gboolean {
            auto* self = static_cast<WebView*>(data);
            if (!self->localFileOnly ||
                (type != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION &&
                 type != WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION)) {
                return FALSE;
            }
            auto* navigation = WEBKIT_NAVIGATION_POLICY_DECISION(decision);
            auto* action = webkit_navigation_policy_decision_get_navigation_action(
                navigation);
            auto* request = webkit_navigation_action_get_request(action);
            const char* uri = webkit_uri_request_get_uri(request);
            if (uri && self->isAllowedLocalDocument(uri)) {
                if (type == WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION) {
                    webkit_web_view_load_uri(view, uri);
                    webkit_policy_decision_ignore(decision);
                    return TRUE;
                }
                return FALSE;
            }
            webkit_policy_decision_ignore(decision);
            g_idle_add(+[](gpointer arg) -> gboolean {
                static_cast<WebView*>(arg)->showForbiddenPage();
                return G_SOURCE_REMOVE;
            }, self);
            return TRUE;
        }),
        this);
    g_signal_connect(G_OBJECT(webview), "load-changed",
                     G_CALLBACK(webview_load_changed_cb), this);
    gtk_container_add(GTK_CONTAINER(scroller), webview);

    g_signal_connect(window, "destroy", G_CALLBACK(destroyWindowCb), this);
    // g_signal_connect(webview, "close", G_CALLBACK(closeWebViewCb), window);

    // Dev Tools if debug
    if (debug) {
        WebKitSettings* settings =
            webkit_web_view_get_settings(WEBKIT_WEB_VIEW(webview));
        webkit_settings_set_enable_write_console_messages_to_stdout(settings,
                                                                    true);
        webkit_settings_set_enable_developer_extras(settings, true);
    } else {
        g_signal_connect(G_OBJECT(webview), "context-menu",
                         G_CALLBACK(webview_context_menu_cb), nullptr);
    }

    webkit_user_content_manager_add_script(
        cm, webkit_user_script_new(
                inject.c_str(), WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
                WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START, NULL, NULL));

    // Monitor for fullscreen changes
    g_signal_connect(G_OBJECT(webview), "enter-fullscreen",
                     G_CALLBACK(webview_enter_fullscreen_cb), this);
    g_signal_connect(G_OBJECT(webview), "leave-fullscreen",
                     G_CALLBACK(webview_leave_fullscreen_cb), this);

    // Done initialization, set properties
    init_done = true;

    setTitle(title);
    if (fullscreen) {
        setFullscreen(true);
    }
    setBgColor(bgR, bgG, bgB, bgA);
    prepareLocalStartPage();
    navigate(url);

    // Finish
    gtk_widget_grab_focus(GTK_WIDGET(webview));
    gtk_widget_show_all(window);

    return 0;
}

void WebView::setTitle(std::string t) {
    if (!init_done) {
        title = t;
    } else {
        gtk_window_set_title(GTK_WINDOW(window), t.c_str());
    }
}

void WebView::setFullscreen(bool fs) {
    if (!init_done) {
        fullscreen = fs;
    } else if (fs) {
        gtk_window_fullscreen(GTK_WINDOW(window));
    } else {
        gtk_window_unfullscreen(GTK_WINDOW(window));
    }
}

void WebView::setFullscreenFromJS(bool allow) { fullscreenFromJS = allow; }

void WebView::setBgColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!init_done) {
        bgR = r;
        bgG = g;
        bgB = b;
        bgA = a;
    } else {
        GdkRGBA color = {r / 255.0, g / 255.0, b / 255.0, a / 255.0};
        webkit_web_view_set_background_color(WEBKIT_WEB_VIEW(webview), &color);
    }
}

bool WebView::run() {
    gtk_main_iteration_do(true);
    return should_exit;
}

void WebView::navigate(std::string u) {
    if (!init_done) {
        url = u;
    } else if (localFileOnly && !isAllowedLocalDocument(u)) {
        showForbiddenPage();
        return;
    } else if (localNotFoundPage && u == url) {
        webkit_web_view_load_html(WEBKIT_WEB_VIEW(webview),
                                  "<title>Not Found</title><h1>Not Found</h1>",
                                  nullptr);
    } else {
        webkit_web_view_load_uri(WEBKIT_WEB_VIEW(webview), u.c_str());
    }
}

void WebView::eval(const std::string& js) {
    while (!ready) {
        g_main_context_iteration(NULL, TRUE);
    }
    js_busy = true;
    webkit_web_view_run_javascript(WEBKIT_WEB_VIEW(webview), js.c_str(), NULL,
                                   webview_eval_finished, this);
    while (js_busy) {
        g_main_context_iteration(NULL, TRUE);
    }
}

void WebView::exit() { should_exit = true; }

void WebView::external_message_received_cb(WebKitUserContentManager*,
                                           WebKitJavascriptResult* r,
                                           gpointer arg) {
    WebView* w = static_cast<WebView*>(arg);
    if (w->js_callback) {
        JSCValue* value = webkit_javascript_result_get_js_value(r);
        std::string str = std::string(jsc_value_to_string(value));
        w->js_callback(*w, str);
    }
}

void WebView::webview_eval_finished(GObject*, GAsyncResult*, gpointer arg) {
    static_cast<WebView*>(arg)->js_busy = false;
}

void WebView::webview_load_changed_cb(WebKitWebView*, WebKitLoadEvent event,
                                      gpointer arg) {
    if (event == WEBKIT_LOAD_FINISHED) {
        static_cast<WebView*>(arg)->ready = true;
    }
}

void WebView::destroyWindowCb(GtkWidget*, gpointer arg) {
    static_cast<WebView*>(arg)->exit();
}

// gboolean WebView::closeWebViewCb(WebKitWebView *webView, GtkWidget *window) {
//   gtk_widget_destroy(window);
//   return TRUE;
// }

gboolean WebView::webview_context_menu_cb(WebKitWebView*, GtkWidget*,
                                          WebKitHitTestResult*, gboolean,
                                          gpointer) {
    // Always hide context menu if not debug
    return TRUE;
}

gboolean WebView::webview_enter_fullscreen_cb(WebKitWebView*, gpointer arg) {
    return !static_cast<WebView*>(arg)->fullscreenFromJS;
}

gboolean WebView::webview_leave_fullscreen_cb(WebKitWebView*, gpointer arg) {
    return !static_cast<WebView*>(arg)->fullscreenFromJS;
}
#endif                      // WEBVIEW_GTK

void WebView::css(const wv::String& css) {
    eval(Str(R"js(
    (
      function (css) {
        if (document.styleSheets.length === 0) {
          var s = document.createElement('style');
          s.type = 'text/css';
          document.head.appendChild(s);
        }
        document.styleSheets[0].insertRule(css);
      }
    )(')js") +
         css + Str("')"));
}

void WebView::setCallback(jscb callback) { js_callback = callback; }

void WebView::preEval(const wv::String& js) {
    inject += Str("(()=>{") + js + Str("})()");  // TODO: is the IIFE necessary?
}

}  // namespace wv

#endif  // WEBVIEW_H
