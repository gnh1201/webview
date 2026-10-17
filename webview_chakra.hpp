#ifndef WEBVIEW_CHAKRA_HPP
#define WEBVIEW_CHAKRA_HPP

#include "webview.hpp"
#include "ChakraCore.h"

#include <algorithm>
#include <limits>
#include <map>

#ifdef WEBVIEW_CHAKRACORE_SCINTILLA
#include <Scintilla.h>
#include <SciLexer.h>
#endif

namespace wv {
namespace detail {
class DebugConsole {
public:
    using RunCallback = std::function<void(const std::wstring&)>;

    DebugConsole() {
        backgroundBrush_ = CreateSolidBrush(RGB(15, 19, 27));
        editorBrush_ = CreateSolidBrush(RGB(18, 24, 34));
        outputBrush_ = CreateSolidBrush(RGB(13, 17, 24));
        textFont_ = CreateFontW(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                FIXED_PITCH | FF_MODERN, L"Cascadia Mono");
        if (!textFont_)
            textFont_ = CreateFontW(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                                    FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                    CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                    FIXED_PITCH | FF_MODERN, L"Consolas");
        uiFont_ = CreateFontW(-16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH, L"Segoe UI");
    }

    ~DebugConsole() {
        if (hwnd_ && IsWindow(hwnd_)) DestroyWindow(hwnd_);
#ifdef WEBVIEW_CHAKRACORE_SCINTILLA
        if (lexillaModule_) FreeLibrary(lexillaModule_);
        if (scintillaModule_) FreeLibrary(scintillaModule_);
#endif
        if (backgroundBrush_) DeleteObject(backgroundBrush_);
        if (editorBrush_) DeleteObject(editorBrush_);
        if (outputBrush_) DeleteObject(outputBrush_);
        if (textFont_) DeleteObject(textFont_);
        if (uiFont_) DeleteObject(uiFont_);
    }

    bool create(RunCallback run) {
        if (hwnd_ && IsWindow(hwnd_)) {
            ShowWindow(hwnd_, SW_SHOW);
            SetForegroundWindow(hwnd_);
            return true;
        }
        run_ = std::move(run);
#ifdef WEBVIEW_CHAKRACORE_SCINTILLA
        if (!loadScintillaModules())
            append(L"Scintilla/Lexilla unavailable; using plain text input.");
#endif
        WNDCLASSW wc{};
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = backgroundBrush_;
        wc.lpszClassName = L"YAWWChakraDebugConsole";
        RegisterClassW(&wc);
        hwnd_ = CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName,
                                L"ChakraCore Console | YAWW",
                                WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                CW_USEDEFAULT, CW_USEDEFAULT, 820, 560,
                                nullptr, nullptr, wc.hInstance, this);
        return hwnd_ != nullptr;
    }

    void append(const std::wstring& line) {
        if (!output_) {
            pendingLines_.push_back(line);
            return;
        }
        appendToOutput(line);
    }

private:
    static constexpr int kInputId = 1;
    static constexpr int kRunId = 2;
    static constexpr int kClearId = 3;
    static constexpr int kOutputId = 4;
    static constexpr COLORREF kTextColor = RGB(218, 226, 239);
    static constexpr COLORREF kMutedColor = RGB(137, 153, 176);

    void createChild(HWND hwnd, HWND& target, const wchar_t* className,
                     const wchar_t* text, DWORD style, int id,
                     HFONT font) {
        target = CreateWindowExW(
            std::wcscmp(className, L"EDIT") == 0 ? WS_EX_CLIENTEDGE : 0,
            className, text,
            WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0, hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
            GetModuleHandleW(nullptr), nullptr);
        if (target && font) SendMessageW(target, WM_SETFONT,
                                        reinterpret_cast<WPARAM>(font), TRUE);
    }

    static void drawButton(const DRAWITEMSTRUCT* item) {
        const bool pressed = (item->itemState & ODS_SELECTED) != 0;
        const bool run = item->CtlID == kRunId;
        const COLORREF fill = run
            ? (pressed ? RGB(43, 113, 190) : RGB(52, 132, 218))
            : (pressed ? RGB(47, 59, 77) : RGB(34, 43, 57));
        HBRUSH brush = CreateSolidBrush(fill);
        FillRect(item->hDC, &item->rcItem, brush);
        DeleteObject(brush);
        HFONT font = reinterpret_cast<HFONT>(SendMessageW(
            item->hwndItem, WM_GETFONT, 0, 0));
        HGDIOBJ oldFont = font ? SelectObject(item->hDC, font) : nullptr;
        SetBkMode(item->hDC, TRANSPARENT);
        SetTextColor(item->hDC, RGB(245, 248, 252));
        wchar_t label[32]{};
        GetWindowTextW(item->hwndItem, label, 32);
        DrawTextW(item->hDC, label, -1, const_cast<RECT*>(&item->rcItem),
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (item->itemState & ODS_FOCUS) {
            RECT focus = item->rcItem;
            InflateRect(&focus, -4, -4);
            DrawFocusRect(item->hDC, &focus);
        }
        if (oldFont) SelectObject(item->hDC, oldFont);
    }

#ifdef WEBVIEW_CHAKRACORE_SCINTILLA
    bool loadScintillaModules() {
        std::wstring executablePath(32768, L'\0');
        const DWORD length = GetModuleFileNameW(
            nullptr, executablePath.data(),
            static_cast<DWORD>(executablePath.size()));
        if (!length || length >= executablePath.size()) return false;
        executablePath.resize(length);
        const auto directory = std::filesystem::path(executablePath).parent_path();
        if (!scintillaModule_)
            scintillaModule_ = LoadLibraryW(
                (directory / L"Scintilla.dll").c_str());
        if (!lexillaModule_)
            lexillaModule_ = LoadLibraryW(
                (directory / L"lexilla.dll").c_str());
        return scintillaModule_ && lexillaModule_ &&
               GetProcAddress(lexillaModule_, "CreateLexer");
    }

    void createInput(HWND hwnd) {
        if (scintillaModule_ && lexillaModule_) {
            input_ = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"Scintilla", L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPCHILDREN |
                    WS_VSCROLL,
                0, 0, 0, 0, hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kInputId)),
                GetModuleHandleW(nullptr), nullptr);
            if (input_) {
                inputIsScintilla_ = true;
                configureScintilla();
                return;
            }
        }
        createChild(hwnd, input_, L"EDIT", L"",
                    WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                    kInputId, textFont_);
    }

    void configureScintilla() {
        SendMessageW(input_, SCI_SETCODEPAGE, SC_CP_UTF8, 0);
        SendMessageW(input_, SCI_STYLESETFONT, STYLE_DEFAULT,
                     reinterpret_cast<LPARAM>("Cascadia Mono"));
        SendMessageW(input_, SCI_STYLESETSIZE, STYLE_DEFAULT, 10);
        SendMessageW(input_, SCI_STYLESETFORE, STYLE_DEFAULT,
                     RGB(218, 226, 239));
        SendMessageW(input_, SCI_STYLESETBACK, STYLE_DEFAULT,
                     RGB(18, 24, 34));
        SendMessageW(input_, SCI_STYLECLEARALL, 0, 0);

        const auto style = [this](int id, COLORREF color, bool bold = false) {
            SendMessageW(input_, SCI_STYLESETFORE, id, color);
            if (bold) SendMessageW(input_, SCI_STYLESETBOLD, id, TRUE);
        };
        style(SCE_C_COMMENT, RGB(106, 153, 115));
        style(SCE_C_COMMENTLINE, RGB(106, 153, 115));
        style(SCE_C_COMMENTDOC, RGB(106, 153, 115));
        style(SCE_C_NUMBER, RGB(181, 206, 168));
        style(SCE_C_WORD, RGB(86, 156, 214), true);
        style(SCE_C_WORD2, RGB(78, 201, 176));
        style(SCE_C_STRING, RGB(206, 145, 120));
        style(SCE_C_STRINGRAW, RGB(206, 145, 120));
        style(SCE_C_CHARACTER, RGB(206, 145, 120));
        style(SCE_C_REGEX, RGB(215, 186, 125));
        style(SCE_C_OPERATOR, RGB(212, 212, 212));
        style(SCE_C_ESCAPESEQUENCE, RGB(220, 180, 120));

        SendMessageW(input_, SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER);
        SendMessageW(input_, SCI_SETMARGINWIDTHN, 0, 42);
        SendMessageW(input_, SCI_SETMARGINBACKN, 0, RGB(15, 20, 29));
        SendMessageW(input_, SCI_STYLESETFORE, STYLE_LINENUMBER,
                     RGB(105, 119, 139));
        SendMessageW(input_, SCI_STYLESETBACK, STYLE_LINENUMBER,
                     RGB(15, 20, 29));
        SendMessageW(input_, SCI_SETCARETFORE, RGB(230, 237, 247), 0);
        SendMessageW(input_, SCI_SETCARETLINEVISIBLE, TRUE, 0);
        SendMessageW(input_, SCI_SETCARETLINEBACK, RGB(28, 37, 51), 0);
        SendMessageW(input_, SCI_SETSELFORE, TRUE, RGB(255, 255, 255));
        SendMessageW(input_, SCI_SETSELBACK, TRUE, RGB(48, 86, 130));
        SendMessageW(input_, SCI_SETWRAPMODE, SC_WRAP_WORD, 0);
        SendMessageW(input_, SCI_SETTABWIDTH, 2, 0);
        SendMessageW(input_, SCI_SETUSETABS, FALSE, 0);
        using CreateLexerFn = void* (__stdcall *)(const char*);
        auto createLexer = reinterpret_cast<CreateLexerFn>(
            GetProcAddress(lexillaModule_, "CreateLexer"));
        void* lexer = createLexer ? createLexer("cpp") : nullptr;
        if (lexer) {
            SendMessageW(input_, SCI_SETILEXER, 0,
                         reinterpret_cast<LPARAM>(lexer));
            SendMessageW(input_, SCI_SETPROPERTY,
                         reinterpret_cast<WPARAM>("lexer.cpp.allow.dollars"),
                         reinterpret_cast<LPARAM>("1"));
            static constexpr char javascriptKeywords[] =
                "abstract arguments async await boolean break byte case catch "
                "char class const continue debugger default delete do double "
                "else enum export extends false final finally float for from "
                "function goto if implements import in instanceof int interface "
                "let long native new null of package private protected public "
                "return short static super switch synchronized this throw "
                "throws transient true try typeof undefined var void volatile "
                "while with yield";
            SendMessageW(input_, SCI_SETKEYWORDS, 0,
                         reinterpret_cast<LPARAM>(javascriptKeywords));
            SendMessageW(input_, SCI_COLOURISE, 0, -1);
        }
    }
#endif

    void appendToOutput(const std::wstring& line) {
        if (!output_) return;
        const int end = GetWindowTextLengthW(output_);
        SendMessageW(output_, EM_SETSEL, end, end);
        SendMessageW(output_, EM_REPLACESEL, FALSE,
                     reinterpret_cast<LPARAM>((line + L"\r\n").c_str()));
    }

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam,
                                       LPARAM lparam) {
        auto* self = reinterpret_cast<DebugConsole*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
            self = static_cast<DebugConsole*>(create->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                              reinterpret_cast<LONG_PTR>(self));
            self->hwnd_ = hwnd;
        }
        if (!self) return DefWindowProcW(hwnd, message, wparam, lparam);
        switch (message) {
            case WM_GETMINMAXINFO: {
                auto* limits = reinterpret_cast<MINMAXINFO*>(lparam);
                limits->ptMinTrackSize.x = 620;
                limits->ptMinTrackSize.y = 450;
                return 0;
            }
            case WM_CREATE:
                self->createChild(hwnd, self->header_, L"STATIC",
                                  L"CHAKRACORE  /  HOST SCRIPT RUNTIME",
                                  SS_LEFT | SS_CENTERIMAGE, 5, self->uiFont_);
                self->createChild(hwnd, self->hint_, L"STATIC",
                                  L"Run JavaScript in the host context. Logs and errors appear below.",
                                  SS_LEFT | SS_CENTERIMAGE, 6, self->uiFont_);
                self->createChild(hwnd, self->output_, L"EDIT", L"",
                                  WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL |
                                      ES_READONLY | ES_NOHIDESEL,
                                  kOutputId, self->textFont_);
#ifdef WEBVIEW_CHAKRACORE_SCINTILLA
                self->createInput(hwnd);
#else
                self->createChild(hwnd, self->input_, L"EDIT", L"",
                                  WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL |
                                      ES_WANTRETURN,
                                  kInputId, self->textFont_);
#endif
                self->createChild(hwnd, self->runButton_, L"BUTTON", L"Run Script",
                                  BS_OWNERDRAW, kRunId, self->uiFont_);
                self->createChild(hwnd, self->clearButton_, L"BUTTON", L"Clear",
                                  BS_OWNERDRAW, kClearId, self->uiFont_);
                if (self->output_) {
                    for (const auto& line : self->pendingLines_)
                        self->appendToOutput(line);
                    self->pendingLines_.clear();
                }
                return 0;
            case WM_SIZE: {
                const int width = LOWORD(lparam);
                const int height = HIWORD(lparam);
                constexpr int pad = 18;
                const int outputTop = 78;
                const int commandHeight = 160;
                const int outputHeight = (std::max)(160, height - 286);
                MoveWindow(self->header_, pad, 14, width - pad * 2, 28, TRUE);
                MoveWindow(self->hint_, pad, 43, width - pad * 2, 22, TRUE);
                MoveWindow(self->output_, pad, outputTop, width - pad * 2,
                           outputHeight, TRUE);
                const int commandTop = outputTop + outputHeight + 12;
                MoveWindow(self->input_, pad, commandTop, width - 166,
                           commandHeight, TRUE);
                MoveWindow(self->runButton_, width - 132, commandTop, 114, 42,
                           TRUE);
                MoveWindow(self->clearButton_, width - 132, commandTop + 50,
                           114, 38, TRUE);
                return 0;
            }
            case WM_ERASEBKGND: {
                RECT rect{};
                GetClientRect(hwnd, &rect);
                FillRect(reinterpret_cast<HDC>(wparam), &rect,
                         self->backgroundBrush_);
                return 1;
            }
            case WM_CTLCOLORSTATIC: {
                HDC dc = reinterpret_cast<HDC>(wparam);
                SetTextColor(dc, reinterpret_cast<HWND>(lparam) == self->hint_
                        ? kMutedColor : kTextColor);
                SetBkColor(dc, RGB(15, 19, 27));
                return reinterpret_cast<LRESULT>(self->backgroundBrush_);
            }
            case WM_CTLCOLOREDIT: {
                HDC dc = reinterpret_cast<HDC>(wparam);
                const HWND control = reinterpret_cast<HWND>(lparam);
                const bool output = control == self->output_;
                SetTextColor(dc, output ? RGB(190, 205, 222) : kTextColor);
                SetBkColor(dc, output ? RGB(13, 17, 24) : RGB(18, 24, 34));
                return reinterpret_cast<LRESULT>(output ? self->outputBrush_
                                                        : self->editorBrush_);
            }
            case WM_DRAWITEM:
                drawButton(reinterpret_cast<DRAWITEMSTRUCT*>(lparam));
                return TRUE;
            case WM_COMMAND:
                if (LOWORD(wparam) == kRunId) {
                    self->executeInput();
                    return 0;
                }
                if (LOWORD(wparam) == kClearId) {
                    SetWindowTextW(self->output_, L"");
                    return 0;
                }
                break;
            case WM_DESTROY:
                self->hwnd_ = nullptr;
                self->output_ = nullptr;
                self->input_ = nullptr;
                self->inputIsScintilla_ = false;
                self->header_ = nullptr;
                self->hint_ = nullptr;
                self->runButton_ = nullptr;
                self->clearButton_ = nullptr;
                return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    void executeInput() {
#ifdef WEBVIEW_CHAKRACORE_SCINTILLA
        if (inputIsScintilla_) {
            const LRESULT byteLength = SendMessageW(input_, SCI_GETLENGTH, 0, 0);
            if (byteLength <= 0 ||
                byteLength > (std::numeric_limits<int>::max)())
                return;
            std::string utf8(static_cast<size_t>(byteLength) + 1, '\0');
            SendMessageW(input_, SCI_GETTEXT, utf8.size(),
                         reinterpret_cast<LPARAM>(utf8.data()));
            const int wideLength = MultiByteToWideChar(
                CP_UTF8, 0, utf8.data(), static_cast<int>(byteLength), nullptr, 0);
            if (wideLength <= 0) return;
            std::wstring source(static_cast<size_t>(wideLength), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                                static_cast<int>(byteLength), source.data(),
                                wideLength);
            if (run_) run_(source);
            return;
        }
#endif
        const int length = GetWindowTextLengthW(input_);
        std::wstring source(static_cast<size_t>(length) + 1, L'\0');
        GetWindowTextW(input_, source.data(), length + 1);
        source.resize(wcslen(source.c_str()));
        if (!source.empty() && run_) run_(source);
    }

    HWND hwnd_ = nullptr;
    HWND output_ = nullptr;
    HWND input_ = nullptr;
    bool inputIsScintilla_ = false;
    HWND header_ = nullptr;
    HWND hint_ = nullptr;
    HWND runButton_ = nullptr;
    HWND clearButton_ = nullptr;
    HBRUSH backgroundBrush_ = nullptr;
    HBRUSH editorBrush_ = nullptr;
    HBRUSH outputBrush_ = nullptr;
    HFONT textFont_ = nullptr;
    HFONT uiFont_ = nullptr;
#ifdef WEBVIEW_CHAKRACORE_SCINTILLA
    HMODULE scintillaModule_ = nullptr;
    HMODULE lexillaModule_ = nullptr;
#endif
    RunCallback run_;
    std::vector<std::wstring> pendingLines_;
};

}  // namespace detail

class ChakraCore {
public:
    struct Options {
        bool enabled = true;
        std::filesystem::path scriptPath;
        bool openDebugConsole = false;
    };

    explicit ChakraCore(WebView& view, Options options = {})
        : view_(view), options_(std::move(options)) {}

    ~ChakraCore() {
        if (systemMenuCommandRegistered_)
            view_.setSystemMenuCommand({}, {});
        if (runtime_) {
            JsSetCurrentContext(context_);
            for (const auto& item : events_)
                for (JsValueRef callback : item.second) JsRelease(callback, nullptr);
            JsSetCurrentContext(JS_INVALID_REFERENCE);
            JsDisposeRuntime(runtime_);
        }
    }

    bool start() {
        if (!options_.enabled) return false;
        view_.setSystemMenuCommand(
            L"Open ChakraCore Console",
            [this]() { showDebugConsole(); });
        systemMenuCommandRegistered_ = true;
        if (options_.openDebugConsole) showDebugConsole();
        if (JsCreateRuntime(JsRuntimeAttributeNone, nullptr, &runtime_) != JsNoError ||
            JsCreateContext(runtime_, &context_) != JsNoError ||
            JsSetCurrentContext(context_) != JsNoError) {
            console_.append(L"Failed to initialize ChakraCore runtime.");
            return false;
        }

        JsValueRef global = nullptr;
        JsValueRef api = nullptr;
        JsCreateObject(&api);
        addMethod(api, L"navigate", Method::Navigate);
        addMethod(api, L"eval", Method::Eval);
        addMethod(api, L"resizeTo", Method::ResizeTo);
        addMethod(api, L"on", Method::On);
        JsGetGlobalObject(&global);
        setProperty(global, L"webview", api);

        JsValueRef consoleObject = nullptr;
        JsCreateObject(&consoleObject);
        addMethod(consoleObject, L"log", Method::Log);
        addMethod(consoleObject, L"error", Method::Log);
        setProperty(global, L"console", consoleObject);

        console_.append(L"ChakraCore host runtime ready.");
        const auto scriptPath = options_.scriptPath.empty()
                                    ? defaultScriptPath()
                                    : options_.scriptPath;
        if (std::filesystem::is_regular_file(scriptPath)) {
            std::ifstream file(scriptPath, std::ios::binary);
            std::string bytes((std::istreambuf_iterator<char>(file)), {});
            const int count = MultiByteToWideChar(CP_UTF8, 0, bytes.data(),
                                                   static_cast<int>(bytes.size()),
                                                   nullptr, 0);
            std::wstring source(static_cast<size_t>(count), L'\0');
            if (count > 0)
                MultiByteToWideChar(CP_UTF8, 0, bytes.data(),
                                    static_cast<int>(bytes.size()), source.data(),
                                    count);
            console_.append(L"Loading " + scriptPath.wstring());
            run(source, scriptPath.filename().wstring());
        } else {
            console_.append(L"ChakraCore script was not found: " +
                            scriptPath.wstring());
        }
        emit(L"ready");
        return true;
    }

    bool showDebugConsole() {
        return console_.create([this](const std::wstring& source) { run(source); });
    }

    void setScriptPath(std::filesystem::path path) {
        options_.scriptPath = std::move(path);
    }

    void setEnabled(bool enabled) { options_.enabled = enabled; }

    void setDebugConsoleEnabled(bool enabled) {
        options_.openDebugConsole = enabled;
    }

    void run(const std::wstring& source, const std::wstring& name = L"console") {
        if (!runtime_ || source.empty()) return;
        JsSetCurrentContext(context_);
        JsValueRef script = makeString(source);
        JsValueRef sourceUrl = makeString(name);
        JsValueRef result = nullptr;
        const JsErrorCode status = JsRun(script, nextSource_++, sourceUrl,
                                         JsParseScriptAttributeNone, &result);
        if (status != JsNoError) {
            reportException(name);
        } else if (name == L"console" && result) {
            JsValueType type = JsUndefined;
            if (JsGetValueType(result, &type) == JsNoError &&
                type != JsUndefined)
                console_.append(valueToString(result));
        }
    }

private:
    static std::filesystem::path defaultScriptPath() {
        std::wstring executablePath(32768, L'\0');
        const DWORD length = GetModuleFileNameW(
            nullptr, executablePath.data(),
            static_cast<DWORD>(executablePath.size()));
        executablePath.resize(length);
        return std::filesystem::path(executablePath).parent_path() /
               L"webview.js";
    }

    enum class Method : uintptr_t { Navigate = 1, Eval, ResizeTo, On, Log };
    static JsValueRef CHAKRA_CALLBACK nativeCall(
        JsValueRef, JsValueRef* args, unsigned short count,
        JsNativeFunctionInfo*, void* state) {
        auto* self = reinterpret_cast<ChakraCore*>(
            reinterpret_cast<uintptr_t>(state) & ~uintptr_t(0x7));
        const auto method = static_cast<Method>(
            reinterpret_cast<uintptr_t>(state) & uintptr_t(0x7));
        if (!self) return JS_INVALID_REFERENCE;
        try {
            // ChakraCore includes thisArg at arguments[0] for native calls;
            // the public host API methods begin at arguments[1].
            return self->invoke(method, count > 0 ? args + 1 : nullptr,
                                count > 0 ? count - 1 : 0);
        } catch (const winrt::hresult_error& error) {
            self->console_.append(
                L"WebView operation failed (HRESULT 0x" +
                formatHresult(static_cast<unsigned long>(error.code().value)) +
                L"): " + error.message().c_str());
            JsValueRef undefined = nullptr;
            JsGetUndefinedValue(&undefined);
            return undefined;
        } catch (...) {
            self->console_.append(L"A native webview operation failed.");
            JsValueRef undefined = nullptr;
            JsGetUndefinedValue(&undefined);
            return undefined;
        }
    }

    static std::wstring formatHresult(unsigned long value) {
        static constexpr wchar_t digits[] = L"0123456789ABCDEF";
        std::wstring result(8, L'0');
        for (int i = 7; i >= 0; --i) {
            result[static_cast<size_t>(i)] = digits[value & 0xF];
            value >>= 4;
        }
        return result;
    }

    void addMethod(JsValueRef object, const wchar_t* name, Method method) {
        const uintptr_t encoded = reinterpret_cast<uintptr_t>(this) |
                                  static_cast<uintptr_t>(method);
        JsValueRef function = nullptr;
        JsPropertyIdRef key = makePropertyId(name);
        JsCreateEnhancedFunction(nativeCall, JS_INVALID_REFERENCE,
                                 reinterpret_cast<void*>(encoded), &function);
        JsSetProperty(object, key, function, true);
    }

    JsValueRef invoke(Method method, JsValueRef* args, unsigned short count) {
        if (method == Method::Log) {
            std::wstring line;
            for (unsigned short i = 0; i < count; ++i) {
                if (i) line += L" ";
                line += valueToString(args[i]);
            }
            console_.append(line);
        } else if (method == Method::Navigate && count > 0) {
            const std::wstring target = valueToString(args[0]);
            emit(L"navigate", target);
            view_.navigate(target);
        } else if (method == Method::Eval && count > 0) {
            view_.eval(valueToString(args[0]));
        } else if (method == Method::ResizeTo && count > 1) {
            double width = 0, height = 0;
            JsNumberToDouble(args[0], &width);
            JsNumberToDouble(args[1], &height);
            view_.resizeTo(static_cast<int>(width), static_cast<int>(height));
        } else if (method == Method::On && count > 1) {
            const std::wstring name = valueToString(args[0]);
            JsValueRef callback = args[1];
            JsAddRef(callback, nullptr);
            events_[name].push_back(callback);
        }
        JsValueRef undefined = nullptr;
        JsGetUndefinedValue(&undefined);
        return undefined;
    }

    void emit(const std::wstring& name, const std::wstring& value = L"") {
        const auto found = events_.find(name);
        if (found == events_.end()) return;
        JsValueRef undefined = nullptr;
        JsGetUndefinedValue(&undefined);
        JsValueRef eventValue = value.empty() ? undefined : makeString(value);
        JsValueRef arguments[2] = {undefined, eventValue};
        for (JsValueRef callback : found->second) {
            JsValueRef result = nullptr;
            if (JsCallFunction(callback, arguments, 2, &result) != JsNoError)
                reportException(L"event:" + name);
        }
    }

    void setProperty(JsValueRef object, const wchar_t* name, JsValueRef value) {
        JsSetProperty(object, makePropertyId(name), value, true);
    }

    static JsValueRef makeString(const std::wstring& value) {
        JsValueRef result = nullptr;
        JsCreateStringUtf16(reinterpret_cast<const uint16_t*>(value.data()),
                            value.size(), &result);
        return result;
    }

    static JsPropertyIdRef makePropertyId(const wchar_t* name) {
        std::string utf8;
        for (const wchar_t* current = name; *current; ++current)
            utf8.push_back(static_cast<char>(*current));
        JsPropertyIdRef result = nullptr;
        JsCreatePropertyId(utf8.data(), utf8.size(), &result);
        return result;
    }

    static std::wstring valueToString(JsValueRef value) {
        JsValueRef string = nullptr;
        int length = 0;
        if (JsConvertValueToString(value, &string) != JsNoError ||
            JsGetStringLength(string, &length) != JsNoError)
            return L"<unprintable>";
        std::wstring result(static_cast<size_t>(length), L'\0');
        size_t written = 0;
        if (JsCopyStringUtf16(
                string, 0, length,
                reinterpret_cast<uint16_t*>(result.data()), &written) !=
            JsNoError)
            return L"<unprintable>";
        result.resize(written);
        return result;
    }

    void reportException(const std::wstring& context) {
        JsValueRef metadata = nullptr;
        if (JsGetAndClearExceptionWithMetadata(&metadata) != JsNoError) {
            console_.append(context + L": script failed");
            return;
        }
        JsPropertyIdRef exceptionProperty = makePropertyId(L"exception");
        JsValueRef exception = nullptr;
        if (JsGetProperty(metadata, exceptionProperty, &exception) != JsNoError)
            exception = metadata;
        console_.append(context + L": " + valueToString(exception));
    }

    WebView& view_;
    Options options_;
    detail::DebugConsole console_;
    bool systemMenuCommandRegistered_ = false;
    JsRuntimeHandle runtime_ = nullptr;
    JsContextRef context_ = nullptr;
    JsSourceContext nextSource_ = 1;
    std::map<std::wstring, std::vector<JsValueRef>> events_;
};

}  // namespace wv

#endif  // WEBVIEW_CHAKRA_HPP
