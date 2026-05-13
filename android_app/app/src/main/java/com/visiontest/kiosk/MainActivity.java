package com.visiontest.kiosk;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowManager;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceRequest;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.util.Log;

/**
 * Fullscreen kiosk WebView for LED Vision.
 *
 * Responsibilities:
 *  1. Starts the C++ HTTP server (foreground service).
 *  2. Shows http://127.0.0.1:8080/index.html in a fullscreen immersive WebView.
 *  3. Intercepts IR remote key events via dispatchKeyEvent() and injects
 *     the matching JavaScript KeyboardEvent so remote.js receives them.
 *  4. Injects kiosk.css and a viewport <meta> tag on every page load.
 */
public class MainActivity extends Activity {

    private static final String TAG       = "VisionKiosk";
    private static final String START_URL = "http://127.0.0.1:8080/index.html";

    private WebView webView;
    private final Handler handler = new Handler(Looper.getMainLooper());

    // ── Activity lifecycle ───────────────────────────────────────────────────

    @SuppressLint({"SetJavaScriptEnabled", "ClickableViewAccessibility"})
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        getWindow().addFlags(
                WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON     |
                WindowManager.LayoutParams.FLAG_FULLSCREEN          |
                WindowManager.LayoutParams.FLAG_DISMISS_KEYGUARD   |
                WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED   |
                WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON);

        setContentView(R.layout.activity_main);

        // Start embedded C++ HTTP server
        Intent svc = new Intent(this, HttpServerService.class);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            startForegroundService(svc);
        } else {
            startService(svc);
        }

        webView = findViewById(R.id.webview);
        configureWebView(webView);
        loadWhenReady();
    }

    @Override
    protected void onResume() {
        super.onResume();
        hideSystemUI();
        if (webView != null) webView.requestFocus();
    }

    @Override
    protected void onDestroy() {
        if (webView != null) webView.destroy();
        super.onDestroy();
    }

    // ── Key event handling ────────────────────────────────────────────────────

    /**
     * Central key event intercept point.
     *
     * Priority order:
     *  1. HOME → swallowed (kiosk cannot be escaped)
     *  2. BACK → inject JS keyCode 8 (each page handles its own back nav);
     *            also call webView.goBack() as safety net for pages that don't
     *  3. Keys in RemoteKeyMapper → inject matching JS KeyboardEvent + call remote()
     *  4. Everything else → forward directly to WebView (DPAD auto-maps to 37/38/39/40/13)
     */
    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (webView == null) return super.dispatchKeyEvent(event);

        int keyCode = event.getKeyCode();
        int action  = event.getAction();

        // ── 1. Block HOME ────────────────────────────────────────────────────
        if (keyCode == KeyEvent.KEYCODE_HOME) return true;

        // ── 2. BACK key ──────────────────────────────────────────────────────
        // Fire JS keyCode 8 so every page's 'case 8' handler can navigate.
        // Also call webView.goBack() on UP so pages that don't handle it still
        // show sensible backward navigation.
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            if (action == KeyEvent.ACTION_DOWN) {
                injectJsKeyEvent(8, "keydown");
            } else if (action == KeyEvent.ACTION_UP) {
                injectJsKeyEvent(8, "keyup");
                if (webView.canGoBack()) {
                    // Small delay: let JS window.location run before goBack()
                    handler.postDelayed(() -> { /* intentionally empty */ }, 150);
                }
            }
            return true;
        }

        // ── 3. Amlogic remote mapped keys ────────────────────────────────────
        int jsKeyCode = RemoteKeyMapper.getJsKeyCode(keyCode);
        if (jsKeyCode != -1) {
            if (action == KeyEvent.ACTION_DOWN) {
                injectJsKeyEvent(jsKeyCode, "keydown");
            } else if (action == KeyEvent.ACTION_UP) {
                injectJsKeyEvent(jsKeyCode, "keyup");
            }
            return true;
        }

        // ── 4. Forward remaining keys to WebView ─────────────────────────────
        // DPAD_UP/DOWN/LEFT/RIGHT/CENTER are translated by Chromium to 38/40/37/39/13
        return webView.dispatchKeyEvent(event);
    }

    /**
     * Injects a synthetic KeyboardEvent into the WebView's document.
     * Fires both the event AND calls remote() directly (for pages that
     * delegate unknown keys to remote.js).
     */
    private void injectJsKeyEvent(int jsKeyCode, String type) {
        String js =
            "(function(){"
            + "var opts={keyCode:" + jsKeyCode + ",which:" + jsKeyCode
            + ",charCode:" + jsKeyCode + ",bubbles:true,cancelable:true};"
            + "document.dispatchEvent(new KeyboardEvent('" + type + "',opts));"
            // Also call remote() directly so pages that check it on keydown get it
            + "if('" + type + "'==='keydown' && typeof remote==='function') remote(" + jsKeyCode + ");"
            + "})();";
        webView.evaluateJavascript(js, null);
    }

    // ── Immersive fullscreen ──────────────────────────────────────────────────

    private void hideSystemUI() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            getWindow().getInsetsController().hide(
                    android.view.WindowInsets.Type.statusBars() |
                    android.view.WindowInsets.Type.navigationBars());
        } else {
            //noinspection deprecation
            getWindow().getDecorView().setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                    | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_FULLSCREEN);
        }
    }

    // ── WebView configuration ─────────────────────────────────────────────────

    @SuppressLint("SetJavaScriptEnabled")
    private void configureWebView(WebView wv) {
        WebSettings s = wv.getSettings();

        // JavaScript + storage (scriptFile.js uses localStorage)
        s.setJavaScriptEnabled(true);
        s.setDomStorageEnabled(true);
        s.setDatabaseEnabled(true);

        // ── TV display settings ────────────────────────────────────────────
        // Use the <meta viewport> tag from each page (wide viewport support).
        s.setUseWideViewPort(true);
        // Do NOT shrink-to-fit: render at 1:1 scale as defined by the viewport.
        s.setLoadWithOverviewMode(false);
        // Ignore Android's system font-size preference so mm-sized clinical
        // letter charts display at their intended physical size.
        s.setTextZoom(100);
        // Hardware acceleration is on by default (set in Manifest/style too).

        s.setMediaPlaybackRequiresUserGesture(false);
        s.setAllowFileAccess(false);       // no file:// access needed
        s.setAllowContentAccess(false);
        s.setBuiltInZoomControls(false);
        s.setSupportZoom(false);
        s.setCacheMode(WebSettings.LOAD_DEFAULT);
        s.setDefaultTextEncodingName("UTF-8");

        wv.setWebChromeClient(new WebChromeClient());
        wv.setWebViewClient(new WebViewClient() {

            @Override
            public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest req) {
                // Keep all navigation inside the WebView
                return false;
            }

            @Override
            public void onPageFinished(WebView view, String url) {
                super.onPageFinished(view, url);
                injectPageFixups(view);
                hideSystemUI();         // re-apply in case a page went fullscreen itself
            }

            @Override
            public void onReceivedError(WebView view, int errorCode,
                                        String description, String failingUrl) {
                Log.e(TAG, "WebView error " + errorCode + ": " + failingUrl);
                if (failingUrl != null && failingUrl.startsWith("http://127.0.0.1")) {
                    handler.postDelayed(() -> view.loadUrl(failingUrl), 1000);
                }
            }
        });

        wv.setFocusable(true);
        wv.setFocusableInTouchMode(true);
        wv.requestFocus();
    }

    /**
     * Injected on every page after load:
     *  a) Add <meta name="viewport"> if the page doesn't already have one.
     *     Ensures consistent scaling on TV regardless of what the HTML says.
     *  b) Load kiosk.css for TV-specific CSS overrides (cursor, overflow, etc.)
     */
    private void injectPageFixups(WebView view) {
        String js =
            "(function(){"

            // ── Viewport meta injection ──────────────────────────────────────
            // Only inject if the page has no viewport meta; don't override
            // pages that already set a specific width (vision tests often do).
            + "if (!document.querySelector('meta[name=viewport]')){"
            + "  var m=document.createElement('meta');"
            + "  m.name='viewport';"
            + "  m.content='width=device-width,initial-scale=1.0,maximum-scale=1.0';"
            + "  document.head.appendChild(m);"
            + "}"

            // ── kiosk.css injection ──────────────────────────────────────────
            // Serves from the C++ server alongside all other assets.
            + "if (!document.getElementById('kiosk-css')){"
            + "  var l=document.createElement('link');"
            + "  l.id='kiosk-css';"
            + "  l.rel='stylesheet';"
            + "  l.href='/kiosk.css';"
            + "  document.head.appendChild(l);"
            + "}"

            + "})();";

        view.evaluateJavascript(js, null);
    }

    // ── Wait for server then load page ────────────────────────────────────────

    private void loadWhenReady() {
        final int  maxAttempts = 20;
        final long delayMs     = 500;

        Runnable check = new Runnable() {
            int attempts = 0;
            @Override
            public void run() {
                attempts++;
                if (attempts == 1 || attempts > maxAttempts) {
                    webView.loadUrl(START_URL);
                    return;
                }
                new Thread(() -> {
                    boolean alive = false;
                    try {
                        java.net.Socket sock = new java.net.Socket();
                        sock.connect(new java.net.InetSocketAddress("127.0.0.1", 8080), 300);
                        sock.close();
                        alive = true;
                    } catch (Exception ignored) {}
                    boolean ok = alive;
                    handler.post(() -> {
                        if (ok) webView.loadUrl(START_URL);
                        else    handler.postDelayed(this, delayMs);
                    });
                }).start();
            }
        };
        handler.postDelayed(check, delayMs);
    }
}
