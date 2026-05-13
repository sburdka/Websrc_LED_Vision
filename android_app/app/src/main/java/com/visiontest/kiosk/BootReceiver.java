package com.visiontest.kiosk;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.util.Log;

/**
 * Starts the Vision Kiosk automatically after device boot.
 * Registered in AndroidManifest.xml for BOOT_COMPLETED and
 * QUICKBOOT_POWERON (some Amlogic firmware uses the latter).
 */
public class BootReceiver extends BroadcastReceiver {

    private static final String TAG = "VisionKiosk";

    @Override
    public void onReceive(Context context, Intent intent) {
        String action = intent.getAction();
        if (Intent.ACTION_BOOT_COMPLETED.equals(action) ||
                "android.intent.action.QUICKBOOT_POWERON".equals(action)) {

            Log.i(TAG, "Boot completed — launching Vision Kiosk");

            // Start the HTTP server service immediately
            Intent serviceIntent = new Intent(context, HttpServerService.class);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                context.startForegroundService(serviceIntent);
            } else {
                context.startService(serviceIntent);
            }

            // Launch the main activity (brings up the WebView)
            Intent activityIntent = new Intent(context, MainActivity.class);
            activityIntent.addFlags(
                    Intent.FLAG_ACTIVITY_NEW_TASK |
                    Intent.FLAG_ACTIVITY_CLEAR_TOP);
            context.startActivity(activityIntent);
        }
    }
}
