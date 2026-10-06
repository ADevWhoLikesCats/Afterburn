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
mat4 game_jet_transform(void);
vec3 game_camera_target(void);
int  game_ready(void);

void game_key_down(const char* key);
void game_key_up(const char* key);

/* Called by main.c when the mouse moves (in pixels, browser coords). */
void game_mouse_move(double x, double y);
/* Called when pointer lock is acquired / released. */
void game_set_pointer_locked(int locked);

#endif
