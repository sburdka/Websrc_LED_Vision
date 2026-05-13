# LED Vision Kiosk — Android App (Amlogic S905W)

Converts the PHP/HTML web project into a self-contained Android kiosk application
that runs on Amlogic S905W devices (or any ARM Android device).

## Architecture

```
┌─────────────────────────────────────────────────────┐
│  Android App (com.visiontest.kiosk)                 │
│                                                     │
│  ┌──────────────────────────────────────────────┐  │
│  │  MainActivity (Java)                         │  │
│  │  • Fullscreen WebView, kiosk mode            │  │
│  │  • Set as HOME launcher (replaces Android UI)│  │
│  │  • Starts on boot via BootReceiver           │  │
│  └──────────────────────────────────────────────┘  │
│                                                     │
│  ┌──────────────────────────────────────────────┐  │
│  │  HttpServerService (Java + C++ via JNI)      │  │
│  │  • Foreground service (always alive)         │  │
│  │  • Extracts web assets on first run          │  │
│  │  • Calls nativeStartServer() → server.cpp    │  │
│  └──────────────────────────────────────────────┘  │
│                                                     │
│  ┌──────────────────────────────────────────────┐  │
│  │  server.cpp (C++ NDK, libvisionkiosk.so)     │  │
│  │  • cpp-httplib HTTP server on 127.0.0.1:8080 │  │
│  │  • Serves static HTML/JS/CSS from assets     │  │
│  │  • /api/serialid  — CPU serial from cpuinfo  │  │
│  │  • /api/sound     — OpenSL ES beep           │  │
│  │  • /api/hdmi      — Amlogic HDMI sysfs       │  │
│  │  • /api/videos    — JSON list of USB videos  │  │
│  │  • /api/images    — JSON list of USB images  │  │
│  │  • /api/importusb — Copy USB files locally   │  │
│  └──────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────┘
        ↑
  WebView opens http://127.0.0.1:8080/index.html
```

## PHP → C++ / HTML Conversion Map

| Original PHP file        | Replaced by                             |
|--------------------------|-----------------------------------------|
| `test1.php`              | `GET /api/sound?data=1` (C++ OpenSL ES) |
| `hdmi.php`               | `GET /api/hdmi?on=1` / `?off=1` (sysfs) |
| `serialid.php`           | `GET /api/serialid` (/proc/cpuinfo)     |
| `check_login.php`        | `check_login.html` (fetch /api/serialid)|
| `poll.php`               | `poll.html` + `GET /api/importusb`      |
| `main.php`               | `main.html` + `GET /api/videos`         |
| `image.php`              | `image.html` + `GET /api/images`        |
| `imageview.php`          | `imageview.html` (pure JS, URL params)  |
| `videoorimage1.php`      | `videoorimage1.html` (static)           |
| `login.php`              | Redirects → `Home.html`                 |
| All other `.php` links   | Redirect shims in server.cpp            |

## Build Instructions

### Prerequisites
- Android Studio Hedgehog (2023.1.1) or newer
- NDK r25c or newer
- CMake 3.22.1+
- Internet access (CMake fetches `cpp-httplib` during first build)

### Steps

1. **Copy web assets into the app:**
   ```bash
   cd /path/to/Websrc_LED_Vision
   bash android_app/copy_web_assets.sh
   ```

2. **Open in Android Studio:**
   - File → Open → select the `android_app/` folder

3. **Build APK:**
   ```
   Build → Build Bundle(s) / APK(s) → Build APK(s)
   ```

4. **Install on S905W device:**
   ```bash
   adb install app/build/outputs/apk/debug/app-debug.apk
   ```

5. **Set as default home launcher:**
   - Go to Android Settings → Apps → Default Apps → Home App → select "LED Vision"

6. **Enable auto-start on boot:**
   - The `BootReceiver` handles this automatically.
   - On some Amlogic firmware you may need to whitelist the app in battery optimisation settings.

## Kiosk / POS Mode

To fully lock the device to this app:

1. Set this app as the **Home** launcher (step 5 above).
2. Disable the **status bar** (requires system/root access):
   ```bash
   adb shell settings put global policy_control immersive.full=*
   ```
3. Optionally use Android Device Policy (MDM) to prevent uninstall.

## Hardware Notes (Amlogic S905W)

| Feature        | Implementation                                         |
|----------------|--------------------------------------------------------|
| Sound beep     | OpenSL ES 1kHz sine wave, 50 ms (no GPIO needed)      |
| HDMI on/off    | `/sys/class/amhdmitx/amhdmitx0/phy` (needs root)      |
| Serial ID      | `/proc/cpuinfo` Serial field                           |
| USB storage    | `/storage/<UUID>/` or `/mnt/media_rw/` mount paths    |
| GPIO (legacy)  | Removed; WiringPi was Raspberry Pi specific            |
