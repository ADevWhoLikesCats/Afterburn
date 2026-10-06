#ifndef GUN_H
#define GUN_H

#include "math3d.h"

#define GUN_MAX_BULLETS    64
#define GUN_FIRE_INTERVAL  0.066f   /* ~15 rounds per second */
#define GUN_BULLET_SPEED   800.0f   /* world units per second */
#define GUN_BULLET_LIFE    0.5f     /* seconds before it disappears */
#define GUN_BULLET_LEN     0.5f     /* visible length in world units */
#define GUN_MUZZLE_LIFE    0.05f    /* muzzle flash duration */

typedef struct {
    vec3  position;
    vec3  velocity;
    float age;
    int   active;
} Bullet;

typedef struct {
    Bullet bullets[GUN_MAX_BULLETS];
    float  fire_cooldown;
    float  muzzle_timer;
    vec3   muzzle_pos;
    int    firing;
} Gun;

void gun_init(Gun* g);
void gun_update(Gun* g, float dt, vec3 origin, vec3 forward, int fire_input);
void gun_render(Gun* g, mat4 viewproj, vec3 cam);
void gun_shutdown(Gun* g);

#endif
