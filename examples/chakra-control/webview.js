// Runs in the host-side ChakraCore runtime, not in the page's JavaScript engine.
webview.on("ready", function () {
  console.log("webview.js is running in ChakraCore");
  webview.navigate("https://example.com");
  webview.resizeTo(1000, 700);
});

webview.on("navigate", function (url) {
  console.log("Navigating to " + url);
});

// The console Run button evaluates snippets. webview.eval() sends code to the
// embedded browser engine and does not return its evaluation result.
console.log("Use webview.navigate(url), webview.eval(js), and webview.resizeTo(w, h)");
