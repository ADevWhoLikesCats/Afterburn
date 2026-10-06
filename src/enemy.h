#ifndef ENEMY_H
#define ENEMY_H

#include "math3d.h"
#include "mesh.h"

#define ENEMY_COUNT 2

typedef struct {
    vec3  position;
    vec3  velocity;
    vec3  forward;
    vec3  up;
    vec3  right;
    float speed_wu;
    int   alive;
    float phase;        /* per-enemy phase offset for Lissajous */
    float omega;        /* per-enemy angular frequency */
} Enemy;

void enemy_init_all(void);
void enemy_update_all(float dt);
int  enemy_count_alive(void);

/* Accessors */
vec3 enemy_position(int i);
vec3 enemy_velocity(int i);
vec3 enemy_forward(int i);
int  enemy_alive(int i);
void enemy_kill(int i);

/* Render: draws all alive enemies using the shared jet model.
   Caller must have already bound the PBR program and set uniforms
   for lighting, SH, SG, shadow, etc. This function reuses the exact
   same uniform layout as game_render. */
void enemy_render_all(int w, int h);

/* Build the world transform for enemy i (same convention as the jet). */
mat4 enemy_model_matrix(int i);

#endif
