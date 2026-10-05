// Local-file-only webview example.
// The app opens index.html from its current working directory. If it is absent,
// the webview displays a built-in Not Found page. Document links to websites
// are blocked and show Forbidden; subresources such as scripts and stylesheets
// are not filtered.

#include "webview.hpp"

WEBVIEW_MAIN {
    wv::WebView w{800, 600, true, true, Str("Local files only")};
    w.setLocalFileOnly(true);

    if (w.init() == -1) {
        return 1;
    }

    while (w.run() == 0)
        ;

    return 0;
}
