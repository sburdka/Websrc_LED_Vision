package com.visiontest.kiosk;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.res.AssetManager;
import android.os.Build;
import android.os.IBinder;
import android.util.Log;

import androidx.core.app.NotificationCompat;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * Foreground service that copies web assets to internal storage and then
 * starts the C++ HTTP server via JNI on port 8080.
 */
public class HttpServerService extends Service {

    private static final String TAG         = "VisionKiosk";
    private static final int    SERVER_PORT = 8080;
    private static final int    NOTIF_ID    = 1001;
    private static final String CHANNEL_ID  = "vision_server_channel";

    private Thread serverThread;

    static {
        System.loadLibrary("visionkiosk");
    }

    // ── JNI declarations ────────────────────────────────────────────────────

    private native void nativeStartServer(String webRoot, String usbDest, int port);
    private native void nativeStopServer();

    // ── Service lifecycle ────────────────────────────────────────────────────

    @Override
    public void onCreate() {
        super.onCreate();
        createNotificationChannel();
        startForeground(NOTIF_ID, buildNotification());
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (serverThread == null || !serverThread.isAlive()) {
            serverThread = new Thread(this::runServer, "CppHttpServer");
            serverThread.setDaemon(true);
            serverThread.start();
        }
        return START_STICKY;
    }

    @Override
    public void onDestroy() {
        nativeStopServer();
        if (serverThread != null) {
            serverThread.interrupt();
        }
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    // ── Server thread ────────────────────────────────────────────────────────

    private void runServer() {
        // Extract web assets from APK to the app's files directory so the
        // C++ server can serve them as static files.
        File webRoot = new File(getFilesDir(), "web");
        File usbDest = new File(getFilesDir(), "usb");
        usbDest.mkdirs();

        try {
            copyAssets(getAssets(), "web", webRoot);
        } catch (IOException e) {
            Log.e(TAG, "Failed to extract web assets: " + e.getMessage());
        }

        Log.i(TAG, "Starting C++ HTTP server: webRoot=" + webRoot.getAbsolutePath());
        // This call blocks until nativeStopServer() is called
        nativeStartServer(webRoot.getAbsolutePath(), usbDest.getAbsolutePath(), SERVER_PORT);
        Log.i(TAG, "C++ HTTP server stopped");
    }

    // ── Asset extraction ─────────────────────────────────────────────────────

    /**
     * Recursively copies assets from the APK into the given destination directory.
     * Only copies files that don't exist yet (no unnecessary I/O on restart).
     */
    private void copyAssets(AssetManager am, String assetPath, File destDir)
            throws IOException {
        String[] list = am.list(assetPath);
        if (list == null) return;

        if (list.length == 0) {
            // This is a file, not a directory
            copyAssetFile(am, assetPath, destDir);
            return;
        }

        destDir.mkdirs();
        for (String child : list) {
            String childAsset = assetPath + "/" + child;
            File   childDest  = new File(destDir, child);
            String[] subList  = am.list(childAsset);
            if (subList != null && subList.length > 0) {
                copyAssets(am, childAsset, childDest);
            } else {
                copyAssetFile(am, childAsset, destDir);
            }
        }
    }

    private void copyAssetFile(AssetManager am, String assetPath, File destDir)
            throws IOException {
        String name = new File(assetPath).getName();
        File   dest = new File(destDir, name);
        if (dest.exists()) return;  // already extracted

        destDir.mkdirs();
        try (InputStream  in  = am.open(assetPath);
             OutputStream out = new FileOutputStream(dest)) {
            byte[] buf = new byte[8192];
            int    n;
            while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
        }
    }

    // ── Notification (required for foreground service) ────────────────────────

    private void createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel ch = new NotificationChannel(
                    CHANNEL_ID,
                    getString(R.string.app_name),
                    NotificationManager.IMPORTANCE_LOW);
            ch.setDescription("Vision test HTTP server");
            NotificationManager nm =
                    (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
            if (nm != null) nm.createNotificationChannel(ch);
        }
    }

    private Notification buildNotification() {
        return new NotificationCompat.Builder(this, CHANNEL_ID)
                .setSmallIcon(android.R.drawable.ic_menu_info_details)
                .setContentTitle(getString(R.string.server_notification_title))
                .setContentText(getString(R.string.server_notification_text))
                .setPriority(NotificationCompat.PRIORITY_LOW)
                .build();
    }
}
