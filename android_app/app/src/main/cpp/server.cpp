/*
 * server.cpp — Embedded HTTP server for LED Vision Kiosk on Amlogic S905W
 *
 * Replaces all PHP backend scripts with C++ HTTP API endpoints.
 * Runs on localhost:8080 via a foreground Android service.
 *
 * API endpoints (mirrors original PHP scripts):
 *   GET /api/serialid          — returns CPU serial from /proc/cpuinfo
 *   GET /api/sound?data=0|1    — plays a beep via OpenSL ES (replaces test1.php)
 *   GET /api/hdmi?on=1         — HDMI display on  (replaces hdmi.php)
 *   GET /api/hdmi?off=1        — HDMI display off
 *   GET /api/videos            — JSON array of MP4 files on USB storage
 *   GET /api/images            — JSON array of JPG/PNG files on USB storage
 *   GET /api/importusb         — detect NTFS/FAT USB, copy files to local dir
 *   GET /                      — serves static web assets (HTML/JS/CSS)
 */

#include "httplib.h"

#include <jni.h>
#include <android/log.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <thread>
#include <atomic>
#include <mutex>

#define LOG_TAG "VisionKiosk"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace fs = std::filesystem;

// ─── globals ────────────────────────────────────────────────────────────────

static std::unique_ptr<httplib::Server> g_server;
static std::atomic<bool>                g_running{false};
static std::string                      g_web_root;
static std::string                      g_usb_dest;   // local copy of USB files

// ─── Serial ID ──────────────────────────────────────────────────────────────

static std::string getSerialId() {
    std::ifstream f("/proc/cpuinfo");
    if (!f) return "0000000000000000";
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("Serial", 0) == 0) {
            auto colon = line.find(':');
            if (colon != std::string::npos) {
                auto serial = line.substr(colon + 2);
                // strip trailing whitespace / newlines
                serial.erase(serial.find_last_not_of(" \t\r\n") + 1);
                return serial;
            }
        }
    }
    return "0000000000000000";
}

// ─── Audio beep via OpenSL ES ───────────────────────────────────────────────

static void playBeep() {
    // 1 kHz sine wave, 50 ms, 16-bit PCM, 44100 Hz mono
    const int    SAMPLE_RATE = 44100;
    const int    DURATION_MS = 50;
    const int    FREQ_HZ     = 1000;
    const int    NUM_SAMPLES = (SAMPLE_RATE * DURATION_MS) / 1000;
    const float  AMP         = 0.5f;

    std::vector<int16_t> buf(NUM_SAMPLES);
    for (int i = 0; i < NUM_SAMPLES; i++) {
        double t = static_cast<double>(i) / SAMPLE_RATE;
        buf[i] = static_cast<int16_t>(AMP * 32767.0 * std::sin(2.0 * M_PI * FREQ_HZ * t));
    }

    SLObjectItf engineObj = nullptr;
    SLEngineItf engine    = nullptr;
    SLObjectItf mixObj    = nullptr;
    SLObjectItf playerObj = nullptr;
    SLPlayItf   player    = nullptr;
    SLAndroidSimpleBufferQueueItf bq = nullptr;

    slCreateEngine(&engineObj, 0, nullptr, 0, nullptr, nullptr);
    (*engineObj)->Realize(engineObj, SL_BOOLEAN_FALSE);
    (*engineObj)->GetInterface(engineObj, SL_IID_ENGINE, &engine);

    (*engine)->CreateOutputMix(engine, &mixObj, 0, nullptr, nullptr);
    (*mixObj)->Realize(mixObj, SL_BOOLEAN_FALSE);

    SLDataLocator_AndroidSimpleBufferQueue bqLoc = {
        SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 1
    };
    SLDataFormat_PCM fmt = {
        SL_DATAFORMAT_PCM,
        1,                              // mono
        SL_SAMPLINGRATE_44_1,
        SL_PCMSAMPLEFORMAT_FIXED_16,
        SL_PCMSAMPLEFORMAT_FIXED_16,
        SL_SPEAKER_FRONT_CENTER,
        SL_BYTEORDER_LITTLEENDIAN
    };
    SLDataSource src  = {&bqLoc, &fmt};
    SLDataLocator_OutputMix outLoc = {SL_DATALOCATOR_OUTPUTMIX, mixObj};
    SLDataSink   sink = {&outLoc, nullptr};

    const SLInterfaceID ids[]  = {SL_IID_BUFFERQUEUE};
    const SLboolean     reqs[] = {SL_BOOLEAN_TRUE};
    (*engine)->CreateAudioPlayer(engine, &playerObj, &src, &sink, 1, ids, reqs);
    (*playerObj)->Realize(playerObj, SL_BOOLEAN_FALSE);
    (*playerObj)->GetInterface(playerObj, SL_IID_PLAY,          &player);
    (*playerObj)->GetInterface(playerObj, SL_IID_BUFFERQUEUE,   &bq);

    (*bq)->Enqueue(bq, buf.data(), static_cast<SLuint32>(buf.size() * sizeof(int16_t)));
    (*player)->SetPlayState(player, SL_PLAYSTATE_PLAYING);

    // Wait for the buffer to drain (~60 ms is plenty)
    std::this_thread::sleep_for(std::chrono::milliseconds(60));

    (*playerObj)->Destroy(playerObj);
    (*mixObj)->Destroy(mixObj);
    (*engineObj)->Destroy(engineObj);
}

// ─── HDMI control (Amlogic S905W sysfs) ─────────────────────────────────────

/*
 * On Amlogic S905W the HDMI PHY is toggled via:
 *   /sys/class/amhdmitx/amhdmitx0/phy   (write "0" = off, "1" = on)
 * Requires the process to have write permission (root or system app).
 * As a fallback we also try the generic framebuffer blank approach.
 */
static bool writeToSysfs(const std::string& path, const std::string& value) {
    std::ofstream f(path);
    if (!f) return false;
    f << value;
    return f.good();
}

static void setHdmi(bool on) {
    const std::string val = on ? "1" : "0";

    // Amlogic-specific node
    if (!writeToSysfs("/sys/class/amhdmitx/amhdmitx0/phy", val)) {
        // Fallback: generic framebuffer blank (0 = unblank, 1 = blank)
        writeToSysfs("/sys/class/graphics/fb0/blank", on ? "0" : "1");
        LOGI("HDMI set via fb0/blank: %s", val.c_str());
    } else {
        LOGI("HDMI set via amhdmitx phy: %s", val.c_str());
    }
}

// ─── File listing ────────────────────────────────────────────────────────────

static void listFilesRecursive(const fs::path& dir,
                                const std::vector<std::string>& exts,
                                std::vector<std::string>& out) {
    std::error_code ec;
    for (auto& entry : fs::recursive_directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        // convert to lower case for comparison
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        for (auto& e : exts) {
            if (ext == e) {
                out.push_back(entry.path().string());
                break;
            }
        }
    }
}

static std::string toJsonArray(const std::vector<std::string>& v) {
    std::ostringstream ss;
    ss << "[";
    for (size_t i = 0; i < v.size(); i++) {
        if (i) ss << ",";
        // Minimal JSON string escaping
        ss << "\"";
        for (char c : v[i]) {
            if (c == '"')  ss << "\\\"";
            else if (c == '\\') ss << "\\\\";
            else ss << c;
        }
        ss << "\"";
    }
    ss << "]";
    return ss.str();
}

/*
 * Returns JSON object: { "flag": 1, "files": [...], "names": [...], "sizes": [...] }
 * Mirrors the data that main.php / image.php used to produce.
 */
static std::string buildFileListJson(const std::vector<std::string>& exts) {
    // Android USB storage is mounted under /storage/ (excluding emulated)
    std::vector<std::string> searchRoots = {
        "/storage",
        "/mnt/media_rw",
        "/mnt/usb",              // some Amlogic firmware
        g_usb_dest               // our local copy after import
    };

    std::vector<std::string> files;
    for (auto& root : searchRoots) {
        if (fs::exists(root)) listFilesRecursive(root, exts, files);
    }
    // Deduplicate
    std::sort(files.begin(), files.end());
    files.erase(std::unique(files.begin(), files.end()), files.end());

    std::ostringstream ss;
    ss << "{\"flag\":" << (files.empty() ? 0 : 1)
       << ",\"files\":" << toJsonArray(files)
       << ",\"count\":" << files.size()
       << "}";
    return ss.str();
}

// ─── USB Import ──────────────────────────────────────────────────────────────

static std::string importUsbFiles() {
    // Look for removable USB volumes under /storage (excludes "emulated")
    std::vector<std::string> usbMounts;
    std::error_code ec;
    if (fs::exists("/storage")) {
        for (auto& entry : fs::directory_iterator("/storage", ec)) {
            auto name = entry.path().filename().string();
            if (name == "emulated" || name == "self") continue;
            if (entry.is_directory()) {
                usbMounts.push_back(entry.path().string());
            }
        }
    }
    // Also check /mnt/media_rw
    if (fs::exists("/mnt/media_rw")) {
        for (auto& entry : fs::directory_iterator("/mnt/media_rw", ec)) {
            if (entry.is_directory()) {
                usbMounts.push_back(entry.path().string());
            }
        }
    }

    if (usbMounts.empty()) {
        return "{\"flag\":0,\"error\":\"No USB storage found\"}";
    }

    // Create destination directory
    fs::create_directories(g_usb_dest, ec);

    int copied = 0;
    for (auto& mount : usbMounts) {
        std::vector<std::string> allFiles;
        listFilesRecursive(mount, {".mp4", ".avi", ".mkv", ".jpg", ".jpeg", ".png"}, allFiles);
        for (auto& src : allFiles) {
            auto dest = fs::path(g_usb_dest) / fs::path(src).filename();
            fs::copy_file(src, dest,
                          fs::copy_options::overwrite_existing, ec);
            if (!ec) copied++;
        }
    }

    std::ostringstream ss;
    ss << "{\"flag\":1,\"copied\":" << copied << "}";
    return ss.str();
}

// ─── HTTP Server setup ───────────────────────────────────────────────────────

static void setupRoutes(httplib::Server& svr) {

    // ── Static file serving ──────────────────────────────────────────────────
    svr.set_mount_point("/", g_web_root);

    // ── API: Serial ID ───────────────────────────────────────────────────────
    svr.Get("/api/serialid", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(getSerialId(), "text/plain");
    });

    // ── API: Beep sound (replaces test1.php) ─────────────────────────────────
    // Called by giveSound() in remote.js: GET /api/sound?data=1
    svr.Get("/api/sound", [](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        auto data = req.get_param_value("data");
        if (data == "1") {
            // Run beep in a detached thread so it doesn't block the response
            std::thread([]{ playBeep(); }).detach();
        }
        res.set_content("ok", "text/plain");
    });

    // ── API: HDMI on/off (replaces hdmi.php) ─────────────────────────────────
    // GET /api/hdmi?on=1  or  GET /api/hdmi?off=1
    svr.Get("/api/hdmi", [](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        if (req.has_param("on")) {
            setHdmi(true);
            res.set_content("Display On", "text/plain");
        } else if (req.has_param("off")) {
            setHdmi(false);
            res.set_content("Display Off", "text/plain");
        } else {
            res.set_content("Missing parameter", "text/plain");
            res.status = 400;
        }
    });

    // ── API: List videos (replaces main.php) ──────────────────────────────────
    // Returns JSON so main.html can build the list dynamically
    svr.Get("/api/videos", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        auto json = buildFileListJson({".mp4", ".avi", ".mkv", ".mov"});
        res.set_content(json, "application/json");
    });

    // ── API: List images (replaces image.php) ─────────────────────────────────
    svr.Get("/api/images", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        auto json = buildFileListJson({".jpg", ".jpeg", ".png", ".bmp"});
        res.set_content(json, "application/json");
    });

    // ── API: USB import (replaces poll.php) ───────────────────────────────────
    svr.Get("/api/importusb", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        // Run import in a thread and return immediately so page can poll
        std::thread([]{ importUsbFiles(); }).detach();
        res.set_content("{\"status\":\"importing\"}", "application/json");
    });

    // ── API: USB import status ────────────────────────────────────────────────
    svr.Get("/api/importusb/status", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        auto json = importUsbFiles(); // synchronous for status check
        res.set_content(json, "application/json");
    });

    // ── Legacy PHP redirect shims ─────────────────────────────────────────────
    // So old href="main.php" links still work by redirecting to .html equivalent
    auto phpRedirect = [](const std::string& target) {
        return [target](const httplib::Request&, httplib::Response& res) {
            res.set_redirect(target);
        };
    };
    svr.Get("/main.php",           phpRedirect("/main.html"));
    svr.Get("/image.php",          phpRedirect("/image.html"));
    svr.Get("/imageview.php",      phpRedirect("/imageview.html"));
    svr.Get("/poll.php",           phpRedirect("/poll.html"));
    svr.Get("/check_login.php",    phpRedirect("/check_login.html"));
    svr.Get("/videoorimage1.php",  phpRedirect("/videoorimage1.html"));
    svr.Get("/home.php",           phpRedirect("/Home.html"));
    svr.Get("/lang_select.php",    phpRedirect("/lang_select.html"));
    svr.Get("/language_sel.php",   phpRedirect("/lang_select.html"));
    svr.Get("/login.php",          phpRedirect("/Home.html"));
    svr.Get("/login_status.php",   phpRedirect("/Home.html"));
    svr.Get("/test1.php",          [](const httplib::Request& req, httplib::Response& res) {
        // Compatibility shim: test1.php?data=1 → same as /api/sound?data=1
        auto data = req.get_param_value("data");
        if (data == "1") std::thread([]{ playBeep(); }).detach();
        res.set_content("", "text/plain");
    });

    svr.set_error_handler([](const httplib::Request& req, httplib::Response& res) {
        LOGE("HTTP 404: %s", req.path.c_str());
        res.set_content("Not Found", "text/plain");
        res.status = 404;
    });
}

// ─── JNI entry points ────────────────────────────────────────────────────────

extern "C" {

JNIEXPORT void JNICALL
Java_com_visiontest_kiosk_HttpServerService_nativeStartServer(
        JNIEnv* env, jobject /*obj*/,
        jstring jWebRoot, jstring jUsbDest, jint port) {

    const char* wr = env->GetStringUTFChars(jWebRoot, nullptr);
    const char* ud = env->GetStringUTFChars(jUsbDest,  nullptr);
    g_web_root = wr ? wr : "/data/local/vision_web";
    g_usb_dest = ud ? ud : "/data/local/vision_usb";
    env->ReleaseStringUTFChars(jWebRoot, wr);
    env->ReleaseStringUTFChars(jUsbDest,  ud);

    LOGI("Starting HTTP server: webroot=%s port=%d", g_web_root.c_str(), (int)port);

    g_server  = std::make_unique<httplib::Server>();
    g_running = true;
    setupRoutes(*g_server);

    // listen() is blocking — the Java service calls us from a background thread
    if (!g_server->listen("127.0.0.1", static_cast<int>(port))) {
        LOGE("Failed to start HTTP server on port %d", (int)port);
        g_running = false;
    }
}

JNIEXPORT void JNICALL
Java_com_visiontest_kiosk_HttpServerService_nativeStopServer(
        JNIEnv* /*env*/, jobject /*obj*/) {
    if (g_server && g_running) {
        LOGI("Stopping HTTP server");
        g_server->stop();
        g_running = false;
    }
}

} // extern "C"
