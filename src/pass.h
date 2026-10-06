#ifndef PASS_H
#define PASS_H

#include <bgfx/c99/bgfx.h>
#include "math3d.h"

typedef struct {
    bgfx_program_handle_t prog;
    bgfx_shader_handle_t  vs;
    bgfx_shader_handle_t  fs;
    int                   valid;
} Pass;

Pass pass_create_from_bin(const char* vert_bin_path, const char* frag_bin_path);
void pass_destroy(Pass* p);
void pass_use(Pass* p);
void pass_draw(Pass* p, int width, int height);

void pass_set_i32 (Pass* p, const char* name, int v);
void pass_set_f32 (Pass* p, const char* name, float v);
void pass_set_vec2(Pass* p, const char* name, vec2 v);
void pass_set_vec3(Pass* p, const char* name, vec3 v);
void pass_set_vec4(Pass* p, const char* name, vec4 v);
void pass_set_mat4(Pass* p, const char* name, mat4 m);
void pass_set_tex (Pass* p, const char* name, int unit);

#endif
