#ifndef GAME_H
#define GAME_H

#include "math3d.h"
#include "mesh.h"

void game_init(void);
void game_prepare_transform(void);
void game_update(float dt);
void game_render(int w, int h);

Mesh* game_jet_meshes(void);
int   game_jet_mesh_count(void);
vec3  game_jet_position(void);
vec3 game_probe_grid_offset(void);
vec3 game_camera_pos(void);
vec3 game_wingtip_left(void);
vec3 game_wingtip_right(void);
vec3 game_camera_target(void);
int  game_ready(void);

/* Missile API */
void  game_fire_track_missile(void);
void  game_fire_burn_missile(void);
int   game_track_missiles(void);
int   game_burn_missiles(void);
int   game_lock_target(void);
float game_lock_progress(void);
int   game_enemy_count(void);
vec3  game_enemy_position(int i);
int   game_enemy_alive(int i);
void  game_render_missiles(mat4 viewproj, vec3 cam);


void game_key_down(const char* key);
void game_key_up(const char* key);

/* Called by main.c when the mouse moves (in pixels, browser coords). */
void game_mouse_move(double x, double y);
void game_mouse_pan(double dx, double dy);
void game_set_pan(int active);
/* Called when pointer lock is acquired / released. */
void game_set_pointer_locked(int locked);

void game_update_wingtips(void);
vec3 game_wingtip_left(void);
vec3 game_wingtip_right(void);

float game_jet_speed(void);
float game_jet_throttle(void);

vec3 game_jet_forward(void);

float game_jet_aoa(void);
float game_jet_g_load(void);
vec3  game_jet_velocity(void);
float game_jet_throttle_raw(void);

#endif
