#include "../../webview.hpp"

#include <shellapi.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    wv::WebView view(900, 650, true, true, L"YAWW ChakraCore",
                     L"about:blank");
    if (view.init() != 0) return 1;

    wv::ChakraCore::Options options;
    options.enabled = true;
    int argumentCount = 0;
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    for (int i = 1; arguments && i < argumentCount; ++i) {
        if (wcscmp(arguments[i], L"--chakra") == 0) {
            options.enabled = true;
        } else if (wcscmp(arguments[i], L"--no-chakra") == 0) {
            options.enabled = false;
        } else if (wcscmp(arguments[i], L"--chakra-console") == 0) {
            options.enabled = true;
            options.openDebugConsole = true;
        } else if (wcscmp(arguments[i], L"--chakra-script") == 0 &&
                   i + 1 < argumentCount) {
            options.enabled = true;
            options.scriptPath = arguments[++i];
        }
    }
    if (arguments) LocalFree(arguments);
    wv::ChakraCore chakra(view, options);
    if (options.enabled) chakra.start();

    while (view.run() == 0) {}
    return 0;
}
