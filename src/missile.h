#ifndef MISSILE_H
#define MISSILE_H

#include "math3d.h"

#define MISSILE_MAX         12
#define MISSILE_PROX_FUSE   25.0f     /* world units */
#define MISSILE_MAX_LIFE    30.0f

typedef enum {
    MISSILE_KIND_TRACK = 0,   /* homing, moderate boost */
    MISSILE_KIND_BURN,        /* ballistic, huge boost */
} MissileKind;

typedef enum {
    MISSILE_PHASE_BOOST = 0,
    MISSILE_PHASE_COAST,
} MissilePhase;

typedef struct {
    vec3  position;
    vec3  velocity;
    vec3  forward;

    MissilePhase phase;
    float phase_time;
    float lifetime;
    float lock_time;

    MissileKind kind;
    int   target_index;      /* -1 = no target */

    /* Config copied at spawn (kind-dependent). */
    float mass_kg;
    float thrust_N;
    float boost_duration;
    float cd_area;
    float max_turn_rate;     /* rad/s for homing missiles */
    float max_range_wu;

    int   active;
} Missile;

typedef struct {
    Missile missiles[MISSILE_MAX];
    int     track_remaining;
    int     burn_remaining;
    float   fire_cooldown;
} MissileBay;

void missile_bay_init(MissileBay* mb);
void missile_fire(MissileBay* mb, MissileKind kind,
                  vec3 launch_pos, vec3 launch_forward, vec3 launch_vel,
                  int target_index);
void missile_update(MissileBay* mb, float dt);
void missile_render(MissileBay* mb, mat4 viewproj, vec3 cam);

/* Hit event queue: main.c / HUD can poll this. */
int  missile_pop_hit(vec3* out_pos, int* out_target);

/* Counts for the HUD. */
int  missile_track_count(const MissileBay* mb);
int  missile_burn_count(const MissileBay* mb);

#endif
