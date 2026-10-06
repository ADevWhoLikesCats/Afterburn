#include "hud.h"
#include <emscripten.h>
#include <stdio.h>

/* Only push to the DOM when values change meaningfully — avoids
   hammering the DOM 60 times a second with identical strings. */
static int   last_speed = -1;
static int   last_alt   = -1;
static int   last_mach  = -1;
static int   last_thr   = -1;
static int   last_msl   = -1;
static int   last_lock  = -1;
static int   last_warn  = -1;

void hud_update(float speed_kts,
                float altitude_ft,
                float mach,
                float throttle_frac,
                int   missile_count,
                int   has_lock,
                int   missile_warning)
{
    int s = (int)(speed_kts + 0.5f);
    int a = (int)(altitude_ft + 0.5f);
    int m = (int)(mach * 100.0f + 0.5f);   /* ×100 so we can compare ints */
    int t = (int)(throttle_frac * 100.0f + 0.5f);
    int w = has_lock ? 1 : 0;
    int wn = missile_warning ? 1 : 0;

    if (s != last_speed) {
        last_speed = s;
        EM_ASM({ document.getElementById('hud-speed').textContent = $0; }, s);
    }
    if (a != last_alt) {
        last_alt = a;
        EM_ASM({ document.getElementById('hud-alt').textContent = $0; }, a);
    }
    if (m != last_mach) {
        last_mach = m;
        EM_ASM({
            document.getElementById('hud-mach').textContent =
                ($0 / 100).toFixed(2);
        }, m);
    }
    if (t != last_thr) {
        last_thr = t;
        EM_ASM({
            document.getElementById('throttle-fill').style.width = $0 + '%';
        }, t);
    }
    if (missile_count != last_msl) {
        last_msl = missile_count;
        EM_ASM({ document.getElementById('hud-msl').textContent = $0; }, missile_count);
    }
    if (w != last_lock) {
        last_lock = w;
        EM_ASM({
            var el = document.getElementById('warn-lock');
            if ($0) el.classList.remove('hidden');
            else    el.classList.add('hidden');
        }, w);
    }
    if (wn != last_warn) {
        last_warn = wn;
        EM_ASM({
            var el = document.getElementById('warn-missile');
            if ($0) el.classList.remove('hidden');
            else    el.classList.add('hidden');
        }, wn);
    }
}
