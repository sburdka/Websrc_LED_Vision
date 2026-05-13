/*
 * server.cpp — Embedded HTTP server for LED Vision Kiosk on Amlogic S905W
 *
 * ALL web content (HTML/JS/CSS/fonts/images) is compiled into this binary
 * as C++ byte arrays via embedded_files.h (generated at build time by
 * generate_assets.py).  There are NO asset files in the APK — the APK
 * assets/ folder is empty.  Extracting the APK yields only this compiled
 * .so, which requires disassembly to read.
 *
 * API endpoints (replace all original PHP scripts):
 *   GET /api/serialid             returns CPU serial from /proc/cpuinfo
 *   GET /api/sound?data=0|1       beep via OpenSL ES   (was test1.php)
 *   GET /api/hdmi?on=1            HDMI on  via sysfs   (was hdmi.php)
 *   GET /api/hdmi?off=1           HDMI off via sysfs
 *   GET /api/videos               JSON list of MP4 on USB storage
 *   GET /api/images               JSON list of JPG/PNG on USB storage
 *   GET /api/importusb            copy USB media to internal dir
 *   GET /api/importusb/status     synchronous USB import + result JSON
 *   GET /<any>                    served from in-memory embedded byte arrays
 */

// embedded_files.h is generated at build time by generate_assets.py
#include "embedded_files.h"

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
#include <unordered_map>

#define LOG_TAG "VisionKiosk"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace fs = std::filesystem;

// ─── globals ─────────────────────────────────────────────────────────────────

static std::unique_ptr<httplib::Server> g_server;
static std::atomic<bool>                g_running{false};
static std::string                      g_usb_dest;

// Immutable lookup map built once at startup from compiled-in byte arrays
static std::unordered_map<std::string, embedded::FileEntry> g_file_map;

// ─── Serial ID ───────────────────────────────────────────────────────────────

static std::string getSerialId() {
    std::ifstream f("/proc/cpuinfo");
    if (!f) return "0000000000000000";
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("Serial", 0) == 0) {
            auto colon = line.find(':');
            if (colon != std::string::npos) {
                auto s = line.substr(colon + 2);
                s.erase(s.find_last_not_of(" \t\r\n") + 1);
                return s;
            }
        }
    }
    return "0000000000000000";
}

// ─── Audio beep via OpenSL ES ─────────────────────────────────────────────────

static void playBeep() {
    const int   SAMPLE_RATE = 44100;
    const int   DURATION_MS = 50;
    const float FREQ_HZ     = 1000.0f;
    const int   NUM_SAMPLES = (SAMPLE_RATE * DURATION_MS) / 1000;

    std::vector<int16_t> buf(NUM_SAMPLES);
    for (int i = 0; i < NUM_SAMPLES; i++) {
        double t = static_cast<double>(i) / SAMPLE_RATE;
        buf[i] = static_cast<int16_t>(0.5 * 32767.0 * std::sin(2.0 * M_PI * FREQ_HZ * t));
    }

    SLObjectItf engineObj = nullptr, mixObj = nullptr, playerObj = nullptr;
    SLEngineItf engine    = nullptr;
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
        SL_DATAFORMAT_PCM, 1, SL_SAMPLINGRATE_44_1,
        SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
        SL_SPEAKER_FRONT_CENTER, SL_BYTEORDER_LITTLEENDIAN
    };
    SLDataSource     src     = {&bqLoc, &fmt};
    SLDataLocator_OutputMix outLoc = {SL_DATALOCATOR_OUTPUTMIX, mixObj};
    SLDataSink       sink    = {&outLoc, nullptr};
    const SLInterfaceID ids[]  = {SL_IID_BUFFERQUEUE};
    const SLboolean     reqs[] = {SL_BOOLEAN_TRUE};

    (*engine)->CreateAudioPlayer(engine, &playerObj, &src, &sink, 1, ids, reqs);
    (*playerObj)->Realize(playerObj, SL_BOOLEAN_FALSE);
    (*playerObj)->GetInterface(playerObj, SL_IID_PLAY,        &player);
    (*playerObj)->GetInterface(playerObj, SL_IID_BUFFERQUEUE, &bq);

    (*bq)->Enqueue(bq, buf.data(), static_cast<SLuint32>(buf.size() * sizeof(int16_t)));
    (*player)->SetPlayState(player, SL_PLAYSTATE_PLAYING);

    std::this_thread::sleep_for(std::chrono::milliseconds(65));

    (*playerObj)->Destroy(playerObj);
    (*mixObj)->Destroy(mixObj);
    (*engineObj)->Destroy(engineObj);
}

// ─── HDMI control (Amlogic S905W sysfs) ──────────────────────────────────────

static bool writeSysfs(const std::string& path, const std::string& val) {
    std::ofstream f(path);
    if (!f) return false;
    f << val;
    return f.good();
}

static void setHdmi(bool on) {
    const std::string val = on ? "1" : "0";
    if (!writeSysfs("/sys/class/amhdmitx/amhdmitx0/phy", val)) {
        writeSysfs("/sys/class/graphics/fb0/blank", on ? "0" : "1");
        LOGI("HDMI via fb0/blank: %s", val.c_str());
    } else {
        LOGI("HDMI via amhdmitx phy: %s", val.c_str());
    }
}

// ─── File listing ─────────────────────────────────────────────────────────────

static void listFilesRecursive(const fs::path& dir,
                                const std::vector<std::string>& exts,
                                std::vector<std::string>& out) {
    std::error_code ec;
    for (auto& e : fs::recursive_directory_iterator(dir, ec)) {
        if (ec) break;
        if (!e.is_regular_file()) continue;
        std::string ext = e.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        for (auto& x : exts) {
            if (ext == x) { out.push_back(e.path().string()); break; }
        }
    }
}

static std::string toJsonArray(const std::vector<std::string>& v) {
    std::ostringstream ss;
    ss << "[";
    for (size_t i = 0; i < v.size(); i++) {
        if (i) ss << ",";
        ss << "\"";
        for (char c : v[i]) {
            if (c == '"') ss << "\\\""; else if (c == '\\') ss << "\\\\"; else ss << c;
        }
        ss << "\"";
    }
    ss << "]";
    return ss.str();
}

// Maps a filesystem extension to its HTTP MIME type
static std::string mimeForExt(const std::string& ext) {
    if (ext == ".mp4")              return "video/mp4";
    if (ext == ".avi")              return "video/x-msvideo";
    if (ext == ".mkv")              return "video/x-matroska";
    if (ext == ".mov")              return "video/quicktime";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".png")              return "image/png";
    if (ext == ".bmp")              return "image/bmp";
    if (ext == ".gif")              return "image/gif";
    if (ext == ".svg")              return "image/svg+xml";
    return "application/octet-stream";
}

// Returns JSON with /media/<filename> URLs — the HTTP server serves them via the
// /media/ route below, so the browser can actually fetch the files.
static std::string buildFileListJson(const std::vector<std::string>& exts) {
    std::vector<std::string> absPaths;

    // 1. Prefer already-imported local copy
    if (!g_usb_dest.empty() && fs::exists(g_usb_dest))
        listFilesRecursive(g_usb_dest, exts, absPaths);

    // 2. Fallback: scan live USB mount points (before import)
    if (absPaths.empty()) {
        for (auto& root : {std::string("/storage"), std::string("/mnt/media_rw"),
                           std::string("/mnt/usb")}) {
            if (fs::exists(root)) listFilesRecursive(root, exts, absPaths);
        }
    }

    std::sort(absPaths.begin(), absPaths.end());
    absPaths.erase(std::unique(absPaths.begin(), absPaths.end()), absPaths.end());

    // Convert absolute paths → /media/<filename> URLs served by this HTTP server
    std::vector<std::string> urls, names;
    for (auto& p : absPaths) {
        auto fname = fs::path(p).filename().string();
        // Percent-encode spaces so the URL is valid
        std::string encoded;
        for (char c : fname) {
            encoded += (c == ' ') ? "%20" : std::string(1, c);
        }
        urls.push_back("/media/" + encoded);
        names.push_back(fname);
    }

    return "{\"flag\":"  + std::string(urls.empty() ? "0" : "1")
         + ",\"files\":" + toJsonArray(urls)
         + ",\"names\":" + toJsonArray(names)
         + ",\"count\":" + std::to_string(urls.size()) + "}";
}

// ─── USB Import ───────────────────────────────────────────────────────────────

static std::string importUsbFiles() {
    std::vector<std::string> mounts;
    std::error_code ec;
    for (auto& root : {std::string("/storage"), std::string("/mnt/media_rw")}) {
        if (!fs::exists(root)) continue;
        for (auto& e : fs::directory_iterator(root, ec)) {
            auto n = e.path().filename().string();
            if (n == "emulated" || n == "self") continue;
            if (e.is_directory()) mounts.push_back(e.path().string());
        }
    }

    if (mounts.empty()) return "{\"flag\":0,\"error\":\"No USB storage found\"}";

    fs::create_directories(g_usb_dest, ec);
    int copied = 0;
    for (auto& mount : mounts) {
        std::vector<std::string> all;
        listFilesRecursive(mount, {".mp4",".avi",".mkv",".jpg",".jpeg",".png"}, all);
        for (auto& src : all) {
            auto dest = fs::path(g_usb_dest) / fs::path(src).filename();
            fs::copy_file(src, dest, fs::copy_options::overwrite_existing, ec);
            if (!ec) copied++;
        }
    }
    return "{\"flag\":1,\"copied\":" + std::to_string(copied) + "}";
}

// ─── In-memory static file handler ───────────────────────────────────────────
// Replaces svr.set_mount_point() — serves from compiled-in byte arrays only.

static void serveEmbeddedFile(const httplib::Request& req, httplib::Response& res) {
    std::string path = req.path;
    if (path == "/" || path.empty()) path = "/index.html";

    // Try exact match first
    auto it = g_file_map.find(path);

    // Fallback: try appending .html (so /check_login → /check_login.html)
    if (it == g_file_map.end()) {
        it = g_file_map.find(path + ".html");
    }

    if (it == g_file_map.end()) {
        LOGE("404: %s", path.c_str());
        res.status = 404;
        res.set_content("Not Found", "text/plain");
        return;
    }

    res.set_content(
        reinterpret_cast<const char*>(it->second.data),
        it->second.len,
        it->second.mime
    );
}

// ─── HTTP routes ──────────────────────────────────────────────────────────────

static void setupRoutes(httplib::Server& svr) {

    // ── API: Serial ID ───────────────────────────────────────────────────────
    svr.Get("/api/serialid", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(getSerialId(), "text/plain");
    });

    // ── API: Beep sound ───────────────────────────────────────────────────────
    svr.Get("/api/sound", [](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        if (req.get_param_value("data") == "1") {
            std::thread([]{ playBeep(); }).detach();
        }
        res.set_content("ok", "text/plain");
    });

    // Legacy shim: test1.php?data=1
    svr.Get("/test1.php", [](const httplib::Request& req, httplib::Response& res) {
        if (req.get_param_value("data") == "1") {
            std::thread([]{ playBeep(); }).detach();
        }
        res.set_content("", "text/plain");
    });

    // ── API: HDMI on/off ──────────────────────────────────────────────────────
    svr.Get("/api/hdmi", [](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        if (req.has_param("on")) {
            setHdmi(true);  res.set_content("Display On",  "text/plain");
        } else if (req.has_param("off")) {
            setHdmi(false); res.set_content("Display Off", "text/plain");
        } else {
            res.status = 400; res.set_content("Bad Request", "text/plain");
        }
    });

    // Legacy shim: hdmi.php?on=ON / ?off=OFF
    svr.Get("/hdmi.php", [](const httplib::Request& req, httplib::Response& res) {
        if (req.has_param("on"))  setHdmi(true);
        if (req.has_param("off")) setHdmi(false);
        res.set_content("", "text/plain");
    });

    // ── API: Video / image lists ───────────────────────────────────────────────
    svr.Get("/api/videos", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(buildFileListJson({".mp4",".avi",".mkv",".mov"}), "application/json");
    });

    svr.Get("/api/images", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(buildFileListJson({".jpg",".jpeg",".png",".bmp"}), "application/json");
    });

    // ── API: USB Import ────────────────────────────────────────────────────────
    svr.Get("/api/importusb", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        std::thread([]{ importUsbFiles(); }).detach();
        res.set_content("{\"status\":\"importing\"}", "application/json");
    });

    svr.Get("/api/importusb/status", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(importUsbFiles(), "application/json");
    });

    // ── Legacy PHP redirect shims ─────────────────────────────────────────────
    auto phpRedirect = [](const std::string& target) {
        return [target](const httplib::Request& req, httplib::Response& res) {
            std::string dest = target;
            // Forward query string so ?id=X&src=Y still reaches the .html page
            if (!req.params.empty()) {
                dest += "?";
                bool first = true;
                for (auto& p : req.params) {
                    if (!first) dest += "&";
                    dest += p.first + "=" + p.second;
                    first = false;
                }
            }
            res.set_redirect(dest);
        };
    };
    svr.Get("/main.php",           phpRedirect("/main.html"));
    svr.Get("/image.php",          phpRedirect("/image.html"));
    svr.Get("/imageview.php",      phpRedirect("/imageview.html"));
    svr.Get("/videoview.php",      phpRedirect("/videoview.html"));
    svr.Get("/videoview1.php",     phpRedirect("/videoview.html"));
    svr.Get("/poll.php",           phpRedirect("/poll.html"));
    svr.Get("/check_login.php",    phpRedirect("/check_login.html"));
    svr.Get("/videoorimage1.php",  phpRedirect("/videoorimage1.html"));
    svr.Get("/home.php",           phpRedirect("/Home.html"));
    svr.Get("/lang_select.php",    phpRedirect("/lang_select.html"));
    svr.Get("/language_sel.php",   phpRedirect("/lang_select.html"));
    svr.Get("/login.php",          phpRedirect("/Home.html"));
    svr.Get("/login_status.php",   phpRedirect("/Home.html"));

    // ── Media streaming: serve video/image files from USB dest or storage ─────
    // Returns the actual bytes of the media file with Range request support so
    // the HTML5 <video> element can seek. URL format: /media/<filename>
    svr.Get("/media/(.*)", [](const httplib::Request& req, httplib::Response& res) {
        // Decode %20 → space; reject path traversal
        std::string raw = req.matches[1].str();
        std::string filename;
        for (size_t i = 0; i < raw.size(); i++) {
            if (raw[i] == '%' && i + 2 < raw.size()) {
                int hi = std::isxdigit(raw[i+1]) ? (std::isdigit(raw[i+1]) ? raw[i+1]-'0' : std::tolower(raw[i+1])-'a'+10) : -1;
                int lo = std::isxdigit(raw[i+2]) ? (std::isdigit(raw[i+2]) ? raw[i+2]-'0' : std::tolower(raw[i+2])-'a'+10) : -1;
                if (hi >= 0 && lo >= 0) { filename += (char)((hi << 4) | lo); i += 2; continue; }
            }
            filename += raw[i];
        }
        // Reject directory traversal
        if (filename.find("..") != std::string::npos || filename.find('/') != std::string::npos) {
            res.status = 403; res.set_content("Forbidden", "text/plain"); return;
        }

        // Locate file: local usb_dest first, then live USB mounts
        std::string filepath;
        auto candidate = g_usb_dest + "/" + filename;
        if (!g_usb_dest.empty() && fs::exists(candidate)) {
            filepath = candidate;
        } else {
            std::string ext = fs::path(filename).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            for (auto& root : {std::string("/storage"), std::string("/mnt/media_rw"),
                               std::string("/mnt/usb")}) {
                std::vector<std::string> found;
                if (fs::exists(root)) listFilesRecursive(root, {ext}, found);
                for (auto& f : found) {
                    if (fs::path(f).filename().string() == filename) {
                        filepath = f; break;
                    }
                }
                if (!filepath.empty()) break;
            }
        }

        if (filepath.empty() || !fs::exists(filepath)) {
            LOGE("Media not found: %s", filename.c_str());
            res.status = 404; res.set_content("Not Found", "text/plain"); return;
        }

        std::string ext = fs::path(filepath).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        std::string mime = mimeForExt(ext);

        std::error_code ec;
        auto file_size = static_cast<size_t>(fs::file_size(filepath, ec));
        if (ec) { res.status = 500; return; }

        res.set_header("Accept-Ranges", "bytes");
        // Use content_provider for streaming — avoids loading the whole file into RAM.
        // cpp-httplib automatically handles Range: bytes=X-Y headers for video seeking.
        res.set_content_provider(
            file_size, mime,
            [filepath](size_t offset, size_t length, httplib::DataSink& sink) -> bool {
                std::ifstream f(filepath, std::ios::binary);
                if (!f) return false;
                f.seekg(static_cast<std::streamoff>(offset));
                char buf[65536];
                size_t remaining = length;
                while (remaining > 0 && f) {
                    size_t n = std::min(remaining, sizeof(buf));
                    f.read(buf, static_cast<std::streamsize>(n));
                    auto got = static_cast<size_t>(f.gcount());
                    if (got == 0) break;
                    if (!sink.write(buf, got)) return false;
                    remaining -= got;
                }
                return true;
            }
        );
        LOGI("Serving media: %s (%zu bytes)", filename.c_str(), file_size);
    });

    // ── All other requests: serve embedded (compiled-in) web files ─────────────
    // This catches every path not matched above, including all .html/.js/.css
    svr.Get("/(.*)", serveEmbeddedFile);

    svr.set_error_handler([](const httplib::Request& req, httplib::Response& res) {
        LOGE("Unhandled: %s", req.path.c_str());
        res.set_content("Not Found", "text/plain");
        res.status = 404;
    });
}

// ─── JNI entry points ─────────────────────────────────────────────────────────

extern "C" {

JNIEXPORT void JNICALL
Java_com_visiontest_kiosk_HttpServerService_nativeStartServer(
        JNIEnv* env, jobject /*obj*/, jstring jUsbDest, jint port) {

    const char* ud = env->GetStringUTFChars(jUsbDest, nullptr);
    g_usb_dest = ud ? ud : "/data/local/vision_usb";
    env->ReleaseStringUTFChars(jUsbDest, ud);

    // Build the in-memory file map once from compiled-in byte arrays
    g_file_map = embedded::buildFileMap();
    LOGI("Loaded %zu embedded web files", g_file_map.size());

    g_server  = std::make_unique<httplib::Server>();
    g_running = true;
    setupRoutes(*g_server);

    LOGI("Starting HTTP server on 127.0.0.1:%d", (int)port);
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
