#ifndef WEBVIEW_CHAKRA_HPP
#define WEBVIEW_CHAKRA_HPP

#include "webview.hpp"
#include "ChakraCore.h"

#include <map>

namespace wv {
namespace detail {
class DebugConsole {
public:
    using RunCallback = std::function<void(const std::wstring&)>;

    ~DebugConsole() {
        if (hwnd_ && IsWindow(hwnd_)) DestroyWindow(hwnd_);
    }

    bool create(RunCallback run) {
        if (hwnd_ && IsWindow(hwnd_)) {
            ShowWindow(hwnd_, SW_SHOW);
            SetForegroundWindow(hwnd_);
            return true;
        }
        run_ = std::move(run);
        WNDCLASSW wc{};
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = L"YAWWChakraDebugConsole";
        RegisterClassW(&wc);
        hwnd_ = CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName,
                                L"YAWW ChakraCore Console",
                                WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                CW_USEDEFAULT, CW_USEDEFAULT, 720, 420,
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
            case WM_CREATE:
                self->output_ = CreateWindowExW(
                    WS_EX_CLIENTEDGE, L"EDIT", L"",
                    WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE |
                        ES_AUTOVSCROLL | ES_READONLY,
                    8, 8, 680, 280, hwnd, nullptr, GetModuleHandleW(nullptr),
                    nullptr);
                self->input_ = CreateWindowExW(
                    WS_EX_CLIENTEDGE, L"EDIT", L"",
                    WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL,
                    8, 296, 570, 72, hwnd,
                    reinterpret_cast<HMENU>(1), GetModuleHandleW(nullptr),
                    nullptr);
                CreateWindowW(L"BUTTON", L"Run",
                              WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                              588, 296, 100, 32, hwnd,
                              reinterpret_cast<HMENU>(2),
                              GetModuleHandleW(nullptr), nullptr);
                if (self->output_) {
                    for (const auto& line : self->pendingLines_)
                        self->appendToOutput(line);
                    self->pendingLines_.clear();
                }
                return 0;
            case WM_SIZE: {
                const int width = LOWORD(lparam);
                const int height = HIWORD(lparam);
                MoveWindow(self->output_, 8, 8, width - 16, height - 124, TRUE);
                MoveWindow(self->input_, 8, height - 108, width - 128, 72, TRUE);
                MoveWindow(GetDlgItem(hwnd, 2), width - 108, height - 108,
                           100, 32, TRUE);
                return 0;
            }
            case WM_COMMAND:
                if (LOWORD(wparam) == 2) {
                    self->executeInput();
                    return 0;
                }
                break;
            case WM_DESTROY:
                self->hwnd_ = nullptr;
                self->output_ = nullptr;
                self->input_ = nullptr;
                return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    void executeInput() {
        const int length = GetWindowTextLengthW(input_);
        std::wstring source(static_cast<size_t>(length) + 1, L'\0');
        GetWindowTextW(input_, source.data(), length + 1);
        source.resize(wcslen(source.c_str()));
        if (!source.empty() && run_) run_(source);
    }

    HWND hwnd_ = nullptr;
    HWND output_ = nullptr;
    HWND input_ = nullptr;
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
        } catch (...) {
            self->console_.append(L"A native webview operation failed.");
            JsValueRef undefined = nullptr;
            JsGetUndefinedValue(&undefined);
            return undefined;
        }
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
