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

void hud_update_lock_box(int visible, int locked, float progress,
                         float sx, float sy, float size);
void hud_update_enemy_arrow(int visible, float sx, float sy, float angle_rad,
                            float distance_m);
void hud_update_missiles(int track, int burn);

#endif
