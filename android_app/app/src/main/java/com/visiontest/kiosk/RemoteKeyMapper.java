package com.visiontest.kiosk;

import android.view.KeyEvent;
import java.util.HashMap;
import java.util.Map;

/**
 * Maps Amlogic S905W IR remote Android key codes to the JavaScript keyCode
 * values expected by remote.js.
 *
 * How remote.js works:
 *   Every HTML page that includes remote.js listens to keydown/keyup and
 *   passes unhandled keys to remote(event.keyCode). The remote() function
 *   switches on these JavaScript keyCodes (mostly ASCII character codes):
 *
 *   65=A HDMI on/off      66=B Help          67=C Mute
 *   68=D Landolt C        70=F Thumbling E   71=G English
 *   72=H Numbers          73=I Astiglan      76=L Snellen
 *   77=M Pediatric        78=N ETDRS         79=O LogMAR
 *   80=P Language 1       81=Q Language 2    82=R Language 3
 *   84=T Duochrome        85=U Settings      86=V Home/Exit
 *   87=W Amsler Grid      88=X Worth 4 Dots  89=Y Phoria
 *   90=Z Illustrated Dots  32=Space Maddox   73=I Astiglan
 *   49=1 H-Coin   50=2 V-Coin   51=3 H-Mask   52=4 V-Mask
 *   53=5 Contrast  54=6 EDU-Chart  56=8 B/W toggle
 *   57=9 Single letter  48=0 Single column
 *   35=# Feet settings  36=$ Lang settings
 *   188=, Ishihara  190=. Vernier Activity
 *
 * Standard navigation keys (up/down/left/right/enter) are handled directly
 * by the WebView's Chromium engine and reach JS as keyCode 38/40/37/39/13.
 * Back (Android KEYCODE_BACK) is injected as JS keyCode 8 by MainActivity.
 */
public class RemoteKeyMapper {

    // Android KeyCode → JavaScript keyCode (remote.js function target)
    private static final Map<Integer, Integer> KEY_MAP = new HashMap<>();

    static {
        // ── System / utility buttons ──────────────────────────────────────────
        KEY_MAP.put(KeyEvent.KEYCODE_MENU,               85);  // U = Settings
        KEY_MAP.put(KeyEvent.KEYCODE_VOLUME_MUTE,        67);  // C = Mute
        KEY_MAP.put(KeyEvent.KEYCODE_INFO,               65);  // A = HDMI toggle
        KEY_MAP.put(KeyEvent.KEYCODE_SEARCH,             85);  // U = Settings

        // ── Coloured buttons (some Amlogic remotes) ────────────────────────────
        KEY_MAP.put(KeyEvent.KEYCODE_PROG_RED,           84);  // T = Duochrome
        KEY_MAP.put(KeyEvent.KEYCODE_PROG_GREEN,         87);  // W = Amsler Grid
        KEY_MAP.put(KeyEvent.KEYCODE_PROG_YELLOW,        88);  // X = Worth 4 Dots
        KEY_MAP.put(KeyEvent.KEYCODE_PROG_BLUE,          86);  // V = Home / Exit

        // ── Media buttons ─────────────────────────────────────────────────────
        KEY_MAP.put(KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE,   76);  // L = Snellen
        KEY_MAP.put(KeyEvent.KEYCODE_MEDIA_STOP,         85);  // U = Settings
        KEY_MAP.put(KeyEvent.KEYCODE_MEDIA_NEXT,         78);  // N = ETDRS
        KEY_MAP.put(KeyEvent.KEYCODE_MEDIA_PREVIOUS,     77);  // M = Pediatric
        KEY_MAP.put(KeyEvent.KEYCODE_MEDIA_FAST_FORWARD, 73);  // I = Astiglan
        KEY_MAP.put(KeyEvent.KEYCODE_MEDIA_REWIND,       89);  // Y = Phoria
        KEY_MAP.put(KeyEvent.KEYCODE_MEDIA_PLAY,         76);  // L = Snellen
        KEY_MAP.put(KeyEvent.KEYCODE_MEDIA_PAUSE,        85);  // U = Settings

        // ── Channel / page buttons ────────────────────────────────────────────
        KEY_MAP.put(KeyEvent.KEYCODE_CHANNEL_UP,         80);  // P = Language 1
        KEY_MAP.put(KeyEvent.KEYCODE_CHANNEL_DOWN,       81);  // Q = Language 2
        KEY_MAP.put(KeyEvent.KEYCODE_PAGE_UP,            80);  // P = Language 1
        KEY_MAP.put(KeyEvent.KEYCODE_PAGE_DOWN,          81);  // Q = Language 2

        // ── Number keys 0–9 (remote numeric keypad) ───────────────────────────
        KEY_MAP.put(KeyEvent.KEYCODE_0,  48);  // 0 = Single column
        KEY_MAP.put(KeyEvent.KEYCODE_1,  49);  // 1 = H-Coin
        KEY_MAP.put(KeyEvent.KEYCODE_2,  50);  // 2 = V-Coin
        KEY_MAP.put(KeyEvent.KEYCODE_3,  51);  // 3 = H-Mask
        KEY_MAP.put(KeyEvent.KEYCODE_4,  52);  // 4 = V-Mask
        KEY_MAP.put(KeyEvent.KEYCODE_5,  53);  // 5 = Contrast Sensitivity
        KEY_MAP.put(KeyEvent.KEYCODE_6,  54);  // 6 = EDU-Chart / Anatomy
        KEY_MAP.put(KeyEvent.KEYCODE_7,  55);  // 7 = Multiline (no action)
        KEY_MAP.put(KeyEvent.KEYCODE_8,  56);  // 8 = B/W Toggle
        KEY_MAP.put(KeyEvent.KEYCODE_9,  57);  // 9 = Single Letter

        // ── Numpad equivalents ────────────────────────────────────────────────
        KEY_MAP.put(KeyEvent.KEYCODE_NUMPAD_0,  48);
        KEY_MAP.put(KeyEvent.KEYCODE_NUMPAD_1,  49);
        KEY_MAP.put(KeyEvent.KEYCODE_NUMPAD_2,  50);
        KEY_MAP.put(KeyEvent.KEYCODE_NUMPAD_3,  51);
        KEY_MAP.put(KeyEvent.KEYCODE_NUMPAD_4,  52);
        KEY_MAP.put(KeyEvent.KEYCODE_NUMPAD_5,  53);
        KEY_MAP.put(KeyEvent.KEYCODE_NUMPAD_6,  54);
        KEY_MAP.put(KeyEvent.KEYCODE_NUMPAD_7,  55);
        KEY_MAP.put(KeyEvent.KEYCODE_NUMPAD_8,  56);
        KEY_MAP.put(KeyEvent.KEYCODE_NUMPAD_9,  57);

        // ── Alphabet keys (if using a USB keyboard remote) ───────────────────
        KEY_MAP.put(KeyEvent.KEYCODE_A, 65);
        KEY_MAP.put(KeyEvent.KEYCODE_B, 66);
        KEY_MAP.put(KeyEvent.KEYCODE_C, 67);
        KEY_MAP.put(KeyEvent.KEYCODE_D, 68);
        KEY_MAP.put(KeyEvent.KEYCODE_F, 70);
        KEY_MAP.put(KeyEvent.KEYCODE_G, 71);
        KEY_MAP.put(KeyEvent.KEYCODE_H, 72);
        KEY_MAP.put(KeyEvent.KEYCODE_I, 73);
        KEY_MAP.put(KeyEvent.KEYCODE_L, 76);
        KEY_MAP.put(KeyEvent.KEYCODE_M, 77);
        KEY_MAP.put(KeyEvent.KEYCODE_N, 78);
        KEY_MAP.put(KeyEvent.KEYCODE_O, 79);
        KEY_MAP.put(KeyEvent.KEYCODE_P, 80);
        KEY_MAP.put(KeyEvent.KEYCODE_Q, 81);
        KEY_MAP.put(KeyEvent.KEYCODE_R, 82);
        KEY_MAP.put(KeyEvent.KEYCODE_S, 83);
        KEY_MAP.put(KeyEvent.KEYCODE_T, 84);
        KEY_MAP.put(KeyEvent.KEYCODE_U, 85);
        KEY_MAP.put(KeyEvent.KEYCODE_V, 86);
        KEY_MAP.put(KeyEvent.KEYCODE_W, 87);
        KEY_MAP.put(KeyEvent.KEYCODE_X, 88);
        KEY_MAP.put(KeyEvent.KEYCODE_Y, 89);
        KEY_MAP.put(KeyEvent.KEYCODE_Z, 90);
        KEY_MAP.put(KeyEvent.KEYCODE_SPACE,    32);
        KEY_MAP.put(KeyEvent.KEYCODE_POUND,    35);   // # = Feet settings
        KEY_MAP.put(KeyEvent.KEYCODE_STAR,     36);   // $ proxy = Lang settings
        KEY_MAP.put(KeyEvent.KEYCODE_COMMA,   188);   // , = Ishihara
        KEY_MAP.put(KeyEvent.KEYCODE_PERIOD,  190);   // . = Vernier Activity
    }

    /**
     * Returns the JS keyCode to inject, or -1 if this key should be forwarded
     * to the WebView unchanged (standard DPAD / Enter handled by Chromium).
     */
    public static int getJsKeyCode(int androidKeyCode) {
        Integer v = KEY_MAP.get(androidKeyCode);
        return v != null ? v : -1;
    }

    public static boolean isMappedKey(int androidKeyCode) {
        return KEY_MAP.containsKey(androidKeyCode);
    }
}
