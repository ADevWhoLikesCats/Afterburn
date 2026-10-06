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


/* ---------- Lock box ---------- */

static int last_lock_visible = -1;
static int last_lock_locked  = -1;
static int last_lock_prog    = -1;
static int last_lock_sx      = -9999;
static int last_lock_sy      = -9999;
static int last_lock_size    = -1;

void hud_update_lock_box(int visible, int locked, float progress,
                         float sx, float sy, float size)
{
    int sx_i = (int)sx;
    int sy_i = (int)sy;
    int sz_i = (int)size;
    int pr_i = (int)(progress * 100.0f);

    if (visible == last_lock_visible &&
        locked  == last_lock_locked  &&
        pr_i    == last_lock_prog    &&
        sx_i    == last_lock_sx      &&
        sy_i    == last_lock_sy      &&
        sz_i    == last_lock_size) return;

    last_lock_visible = visible;
    last_lock_locked  = locked;
    last_lock_prog    = pr_i;
    last_lock_sx      = sx_i;
    last_lock_sy      = sy_i;
    last_lock_size    = sz_i;

    EM_ASM({
        var box = document.getElementById('lock-box');
        if (!$0) { box.classList.add('hidden'); return; }
        box.classList.remove('hidden');
        var sz = $3;
        box.style.left   = ($1 - sz/2) + 'px';
        box.style.top    = ($2 - sz/2) + 'px';
        box.style.width  = sz + 'px';
        box.style.height = sz + 'px';
        if ($4) {
            box.classList.add('locked');
            box.classList.remove('acquiring');
            document.getElementById('lock-text').textContent = 'LOCK';
        } else if ($5 > 0) {
            box.classList.remove('locked');
            box.classList.add('acquiring');
            document.getElementById('lock-text').textContent = 'ACQ ' + $5 + '%';
        } else {
            box.classList.remove('locked');
            box.classList.remove('acquiring');
            document.getElementById('lock-text').textContent = 'SEEK';
        }
    }, visible, sx_i, sy_i, sz_i, locked, pr_i);
}

/* ---------- Off-screen enemy arrow ---------- */

static int last_arrow_visible = -1;
static int last_arrow_x = -9999;
static int last_arrow_y = -9999;
static int last_arrow_angle = -9999;
static int last_arrow_dist = -1;

void hud_update_enemy_arrow(int visible, float sx, float sy, float angle_deg,
                            float distance_m)
{
    int x = (int)sx;
    int y = (int)sy;
    int a = (int)angle_deg;
    int d = (int)(distance_m / 10.0f);

    if (visible == last_arrow_visible &&
        x == last_arrow_x && y == last_arrow_y &&
        a == last_arrow_angle && d == last_arrow_dist) return;

    last_arrow_visible = visible;
    last_arrow_x = x;
    last_arrow_y = y;
    last_arrow_angle = a;
    last_arrow_dist = d;

    EM_ASM({
        var el = document.getElementById('enemy-arrow');
        if (!$0) { el.classList.add('hidden'); return; }
        el.classList.remove('hidden');
        el.style.left = $1 + 'px';
        el.style.top  = $2 + 'px';
        var shape = el.querySelector('.arrow-shape');
        shape.style.transform = 'rotate(' + $3 + 'deg)';
        document.getElementById('enemy-arrow-dist').textContent =
            ($4 * 10) + 'm';
    }, visible, x, y, a, d);
}

/* ---------- Missile counters ---------- */

static int last_track_count = -1;
static int last_burn_count  = -1;

void hud_update_missiles(int track, int burn)
{
    if (track == last_track_count && burn == last_burn_count) return;
    last_track_count = track;
    last_burn_count  = burn;
    EM_ASM({
        document.getElementById('hud-track-count').textContent = $0;
        document.getElementById('hud-burn-count').textContent  = $1;
    }, track, burn);
}
