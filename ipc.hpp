#ifndef WEBVIEW_IPC_H
#define WEBVIEW_IPC_H

#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <cstdio>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace wv {
namespace detail {

// Newline-delimited JSON-RPC 2.0 transport and MCP client used by WebView.
class StdioMcpClient {
public:
    StdioMcpClient() = default;
    StdioMcpClient(const StdioMcpClient&) = delete;
    StdioMcpClient& operator=(const StdioMcpClient&) = delete;
    ~StdioMcpClient() { disconnect(); }

    void connect(const std::string& executable,
                 const std::vector<std::string>& arguments) {
        disconnect();
#if defined(_WIN32)
        SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
        HANDLE childIn = nullptr, childOut = nullptr;
        if (!CreatePipe(&childIn, &stdin_, &sa, 0))
            throw std::runtime_error("could not create process stdin pipe");
        if (!CreatePipe(&stdout_, &childOut, &sa, 0)) {
            CloseHandle(childIn); CloseHandle(stdin_); stdin_ = nullptr;
            throw std::runtime_error("could not create process stdout pipe");
        }
        SetHandleInformation(stdin_, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(stdout_, HANDLE_FLAG_INHERIT, 0);
        std::wstring command = quote(utf8ToWide(executable));
        for (const auto& arg : arguments) command += L" " + quote(utf8ToWide(arg));
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = childIn;
        startup.hStdOutput = childOut;
        startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        PROCESS_INFORMATION process{};
        const BOOL started = CreateProcessW(nullptr, command.data(), nullptr,
            nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
        CloseHandle(childIn);
        CloseHandle(childOut);
        if (!started) {
            disconnect();
            throw std::runtime_error("could not start stdio process");
        }
        process_ = process.hProcess;
        CloseHandle(process.hThread);
#else
        int toChild[2], fromChild[2];
        if (pipe(toChild) != 0) throw std::runtime_error("could not create process stdin pipe");
        if (pipe(fromChild) != 0) {
            close(toChild[0]); close(toChild[1]);
            throw std::runtime_error("could not create process stdout pipe");
        }
        pid_ = fork();
        if (pid_ < 0) {
            close(toChild[0]); close(toChild[1]); close(fromChild[0]); close(fromChild[1]);
            throw std::runtime_error("could not start stdio process");
        }
        if (pid_ == 0) {
            dup2(toChild[0], STDIN_FILENO);
            dup2(fromChild[1], STDOUT_FILENO);
            close(toChild[0]); close(toChild[1]); close(fromChild[0]); close(fromChild[1]);
            std::vector<char*> argv{const_cast<char*>(executable.c_str())};
            for (const auto& arg : arguments) argv.push_back(const_cast<char*>(arg.c_str()));
            argv.push_back(nullptr);
            execvp(executable.c_str(), argv.data());
            _exit(127);
        }
        close(toChild[0]); close(fromChild[1]);
        input_ = fdopen(toChild[1], "w");
        output_ = fdopen(fromChild[0], "r");
        if (!input_ || !output_) {
            disconnect();
            throw std::runtime_error("could not open process stdio pipes");
        }
        setvbuf(input_, nullptr, _IONBF, 0);
#endif
        nextId_ = 1;
        request("initialize", nlohmann::json{
            {"protocolVersion", "2025-11-25"},
            {"capabilities", nlohmann::json::object()},
            {"clientInfo", {{"name", "webview"}, {"version", "1.0.0"}}}
        });
        notify("notifications/initialized", nlohmann::json::object());
    }

    std::string callTool(const std::string& name,
                         const nlohmann::json& arguments) {
        if (!arguments.is_object())
            throw std::runtime_error("MCP tool arguments must be a JSON object");
        return request("tools/call", {{"name", name}, {"arguments", arguments}});
    }

    std::string send(const std::string& method, const std::string& paramsJson) {
        return request(method, nlohmann::json::parse(paramsJson));
    }

    bool connected() const {
#if defined(_WIN32)
        return process_ && stdin_ && stdout_;
#else
        return pid_ > 0 && input_ && output_;
#endif
    }

    void disconnect() {
#if defined(_WIN32)
        if (stdin_) { CloseHandle(stdin_); stdin_ = nullptr; }
        if (stdout_) { CloseHandle(stdout_); stdout_ = nullptr; }
        if (process_) {
            if (WaitForSingleObject(process_, 1000) == WAIT_TIMEOUT)
                TerminateProcess(process_, 0);
            CloseHandle(process_);
            process_ = nullptr;
        }
#else
        if (input_) { std::fclose(input_); input_ = nullptr; }
        if (output_) { std::fclose(output_); output_ = nullptr; }
        if (pid_ > 0) {
            int status = 0;
            if (waitpid(pid_, &status, WNOHANG) == 0) {
                kill(pid_, SIGTERM);
                waitpid(pid_, &status, 0);
            }
            pid_ = -1;
        }
#endif
    }

private:
    std::string request(const std::string& method, const nlohmann::json& params) {
        if (!connected()) throw std::runtime_error("stdio process is not connected");
        const auto id = nextId_++;
        writeLine(nlohmann::json{
            {"jsonrpc", "2.0"}, {"id", id}, {"method", method}, {"params", params}
        }.dump() + "\n");

        for (;;) {
            const auto response = nlohmann::json::parse(readLine());
            if (!response.is_object())
                throw std::runtime_error("JSON-RPC response must be an object");
            // Notifications from the server do not carry an id and cannot answer this request.
            if (!response.contains("id")) continue;
            if (!response.contains("jsonrpc") || response.at("jsonrpc") != "2.0")
                throw std::runtime_error("invalid JSON-RPC response version");
            if (response.at("id") != id)
                throw std::runtime_error("JSON-RPC response id did not match request");
            if (response.contains("error")) {
                const auto& error = response.at("error");
                const auto message = error.is_object() && error.contains("message") &&
                                             error.at("message").is_string()
                                         ? error.at("message").get<std::string>()
                                         : error.dump();
                throw std::runtime_error("JSON-RPC error: " + message);
            }
            if (!response.contains("result"))
                throw std::runtime_error("JSON-RPC response has no result");
            return response.at("result").dump();
        }
    }

    void notify(const std::string& method, const nlohmann::json& params) {
        writeLine(nlohmann::json{
            {"jsonrpc", "2.0"}, {"method", method}, {"params", params}
        }.dump() + "\n");
    }

    void writeLine(const std::string& line) {
#if defined(_WIN32)
        DWORD written = 0;
        if (!WriteFile(stdin_, line.data(), static_cast<DWORD>(line.size()), &written, nullptr) || written != line.size())
            throw std::runtime_error("could not write JSON-RPC message");
#else
        if (std::fputs(line.c_str(), input_) == EOF)
            throw std::runtime_error("could not write JSON-RPC message");
#endif
    }

    std::string readLine() {
        std::string line;
#if defined(_WIN32)
        char c = 0;
        DWORD count = 0;
        while (ReadFile(stdout_, &c, 1, &count, nullptr) && count && c != '\n') line += c;
        if (!count) throw std::runtime_error("stdio process closed stdout");
#else
        int c = 0;
        while ((c = std::fgetc(output_)) != EOF && c != '\n') line += static_cast<char>(c);
        if (c == EOF) throw std::runtime_error("stdio process closed stdout");
#endif
        return line;
    }

#if defined(_WIN32)
    static std::wstring utf8ToWide(const std::string& value) {
        if (value.empty()) return {};
        const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                             value.data(), static_cast<int>(value.size()),
                                             nullptr, 0);
        if (size <= 0) throw std::runtime_error("process path or argument is not valid UTF-8");
        std::wstring result(static_cast<size_t>(size), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                            static_cast<int>(value.size()), result.data(), size);
        return result;
    }

    static std::wstring quote(const std::wstring& arg) {
        std::wstring out = L"\"";
        size_t slashes = 0;
        for (wchar_t c : arg) {
            if (c == L'\\') { ++slashes; continue; }
            if (c == L'"') out.append(slashes * 2 + 1, L'\\');
            else out.append(slashes, L'\\');
            out += c;
            slashes = 0;
        }
        out.append(slashes * 2, L'\\');
        return out + L'"';
    }
    HANDLE process_ = nullptr, stdin_ = nullptr, stdout_ = nullptr;
#else
    FILE* input_ = nullptr;
    FILE* output_ = nullptr;
    pid_t pid_ = -1;
#endif
    unsigned nextId_ = 1;
};

} // namespace detail
} // namespace wv
#endif
