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
 * Fullscreen kiosk WebView.
 *
 * On boot (or manual launch) this activity:
 *  1. Starts the C++ HTTP server as a foreground service.
 *  2. Opens http://127.0.0.1:8080/index.html in a fullscreen, immersive WebView.
 *  3. Disables the back button so users cannot escape the app.
 */
public class MainActivity extends Activity {

    private static final String TAG       = "VisionKiosk";
    private static final String START_URL = "http://127.0.0.1:8080/index.html";

    private WebView webView;
    private Handler handler = new Handler(Looper.getMainLooper());

    // ── Activity lifecycle ───────────────────────────────────────────────────

    @SuppressLint({"SetJavaScriptEnabled", "ClickableViewAccessibility"})
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Fullscreen + keep screen on
        getWindow().addFlags(
                WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON |
                WindowManager.LayoutParams.FLAG_FULLSCREEN |
                WindowManager.LayoutParams.FLAG_DISMISS_KEYGUARD |
                WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED |
                WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON);

        setContentView(R.layout.activity_main);

        // Start the embedded HTTP server service
        startService(new Intent(this, HttpServerService.class));

        webView = findViewById(R.id.webview);
        configureWebView(webView);

        // The C++ server needs a moment to bind the port on first launch.
        // Poll until it's ready, then load the page.
        loadWhenReady();
    }

    @Override
    protected void onResume() {
        super.onResume();
        hideSystemUI();
    }

    @Override
    protected void onDestroy() {
        if (webView != null) {
            webView.destroy();
        }
        super.onDestroy();
    }

    // Prevent back-button from closing the kiosk
    @Override
    public void onBackPressed() {
        if (webView != null && webView.canGoBack()) {
            webView.goBack();
        }
        // else: do nothing — trap user in the app
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        // Block HOME key (requires this activity to be the default home app)
        if (keyCode == KeyEvent.KEYCODE_HOME) return true;
        return super.onKeyDown(keyCode, event);
    }

    // ── Immersive / sticky fullscreen ────────────────────────────────────────

    private void hideSystemUI() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            getWindow().getInsetsController().hide(
                    android.view.WindowInsets.Type.statusBars() |
                    android.view.WindowInsets.Type.navigationBars());
        } else {
            getWindow().getDecorView().setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                    | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_FULLSCREEN);
        }
    }

    // ── WebView setup ────────────────────────────────────────────────────────

    @SuppressLint("SetJavaScriptEnabled")
    private void configureWebView(WebView wv) {
        WebSettings s = wv.getSettings();
        s.setJavaScriptEnabled(true);
        s.setDomStorageEnabled(true);       // localStorage (scriptFile.js uses this)
        s.setDatabaseEnabled(true);
        s.setMediaPlaybackRequiresUserGesture(false);
        s.setAllowFileAccess(true);
        s.setAllowContentAccess(true);
        s.setBuiltInZoomControls(false);
        s.setSupportZoom(false);
        s.setLoadWithOverviewMode(true);
        s.setUseWideViewPort(true);
        s.setCacheMode(WebSettings.LOAD_DEFAULT);

        wv.setWebChromeClient(new WebChromeClient());
        wv.setWebViewClient(new WebViewClient() {
            @Override
            public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {
                // Keep all navigation inside the WebView
                return false;
            }

            @Override
            public void onReceivedError(WebView view, int errorCode,
                                        String description, String failingUrl) {
                Log.e(TAG, "WebView error " + errorCode + " on " + failingUrl);
                // Retry after a short delay if the server isn't up yet
                if (failingUrl != null && failingUrl.startsWith("http://127.0.0.1")) {
                    handler.postDelayed(() -> view.loadUrl(failingUrl), 1000);
                }
            }
        });

        wv.setFocusable(true);
        wv.setFocusableInTouchMode(true);
        wv.requestFocus();
    }

    // ── Wait for server then load page ───────────────────────────────────────

    private void loadWhenReady() {
        // Try to connect to the local server; retry every 500 ms for up to 10 s
        final int maxAttempts = 20;
        final long delayMs    = 500;

        Runnable check = new Runnable() {
            int attempts = 0;
            @Override
            public void run() {
                attempts++;
                // The WebViewClient.onReceivedError handler will retry on failure,
                // so just load immediately. The retry loop above is a safety net.
                if (attempts == 1 || attempts > maxAttempts) {
                    webView.loadUrl(START_URL);
                    return;
                }
                // Simple liveness check via a background thread
                new Thread(() -> {
                    boolean alive = false;
                    try {
                        java.net.Socket s = new java.net.Socket();
                        s.connect(new java.net.InetSocketAddress("127.0.0.1", 8080), 300);
                        s.close();
                        alive = true;
                    } catch (Exception ignored) {}
                    boolean finalAlive = alive;
                    handler.post(() -> {
                        if (finalAlive) {
                            webView.loadUrl(START_URL);
                        } else {
                            handler.postDelayed(this, delayMs);
                        }
                    });
                }).start();
            }
        };
        handler.postDelayed(check, delayMs);
    }
}
