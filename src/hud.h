#ifndef HUD_H
#define HUD_H

/* Per-frame update. Call from the frame loop after game_update. */
void hud_update(float speed_kts,
                float altitude_ft,
                float mach,
                float throttle_frac,   /* 0..1 */
                int   missile_count,
                int   has_lock,
                int   missile_warning);

#endif
