package com.visiontest.kiosk;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.IBinder;
import android.util.Log;

import androidx.core.app.NotificationCompat;

import java.io.File;

/**
 * Foreground service that runs the embedded C++ HTTP server on port 8080.
 *
 * All web content (HTML/JS/CSS/fonts/images) is compiled into the native
 * library as byte arrays — there are NO asset files to extract or copy.
 * The server serves everything from memory.
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

    // JNI: signature matches server.cpp extern "C" declarations
    private native void nativeStartServer(String usbDest, int port);
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
        if (serverThread != null) serverThread.interrupt();
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) { return null; }

    // ── Server thread ────────────────────────────────────────────────────────

    private void runServer() {
        // USB destination: writable internal directory for media copied from USB
        File usbDest = new File(getFilesDir(), "usb");
        usbDest.mkdirs();

        Log.i(TAG, "Starting embedded C++ HTTP server (all assets compiled-in)");

        // Blocks until nativeStopServer() is called
        nativeStartServer(usbDest.getAbsolutePath(), SERVER_PORT);

        Log.i(TAG, "C++ HTTP server stopped");
    }

    // ── Notification (required for foreground service) ────────────────────────

    private void createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel ch = new NotificationChannel(
                    CHANNEL_ID,
                    getString(R.string.app_name),
                    NotificationManager.IMPORTANCE_LOW);
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
