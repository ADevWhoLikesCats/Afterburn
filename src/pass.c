#include "pass.h"
#include "asset.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void* load_shader_bin(const char* path, long* out_size) {
    if (!path || !path[0]) { *out_size = 0; return NULL; }
    size_t sz = 0;
    char* buf = asset_load(path, &sz);
    if (!buf) {
        fprintf(stderr, "pass: cannot load shader bin %s\n", path);
        *out_size = 0;
        return NULL;
    }
    *out_size = (long)sz;
    return buf;
}

Pass pass_create_from_bin(const char* vert_bin_path, const char* frag_bin_path) {
    Pass p;
    memset(&p, 0, sizeof(p));
    p.valid = 0;

    long vs_sz = 0, fs_sz = 0;
    void* vs_buf = load_shader_bin(vert_bin_path, &vs_sz);
    void* fs_buf = load_shader_bin(frag_bin_path, &fs_sz);

    if (vs_buf) {
        p.vs = bgfx_create_shader(bgfx_copy(vs_buf, (uint32_t)vs_sz));
        free(vs_buf);
    }
    if (fs_buf) {
        p.fs = bgfx_create_shader(bgfx_copy(fs_buf, (uint32_t)fs_sz));
        free(fs_buf);
    }

    if (!p.vs.idx || p.vs.idx == UINT16_MAX) {
        fprintf(stderr, "pass: vertex shader creation failed for %s\n", vert_bin_path);
        return p;
    }
    if (frag_bin_path && frag_bin_path[0] &&
        (!p.fs.idx || p.fs.idx == UINT16_MAX)) {
        fprintf(stderr, "pass: fragment shader creation failed for %s\n", frag_bin_path);
        return p;
    }

    p.prog = bgfx_create_program(p.vs, p.fs, true);
    if (!p.prog.idx || p.prog.idx == UINT16_MAX) {
        fprintf(stderr, "pass: program creation failed\n");
        return p;
    }

    p.valid = 1;
    fprintf(stderr, "[pass] created vs=%u fs=%u prog=%u\n",
            p.vs.idx, p.fs.idx, p.prog.idx);
    return p;
}

void pass_destroy(Pass* p) {
    if (!p) return;
    if (p->prog.idx && p->prog.idx != UINT16_MAX) {
        bgfx_destroy_program(p->prog);
    }
    p->prog.idx = UINT16_MAX;
    p->vs.idx   = UINT16_MAX;
    p->fs.idx   = UINT16_MAX;
    p->valid    = 0;
}

void pass_use(Pass* p) {
    (void)p;
}

void pass_draw(Pass* p, int width, int height) {
    (void)p; (void)width; (void)height;
    bgfx_set_vertex_count(3);
}

void pass_set_i32(Pass* p, const char* name, int v) {
    (void)p;
    bgfx_uniform_handle_t u = bgfx_create_uniform(name, BGFX_UNIFORM_TYPE_VEC4, 1);
    float fv[4] = { (float)v, 0.0f, 0.0f, 0.0f };
    bgfx_set_uniform(u, fv, 1);
}

void pass_set_f32(Pass* p, const char* name, float v) {
    (void)p;
    bgfx_uniform_handle_t u = bgfx_create_uniform(name, BGFX_UNIFORM_TYPE_VEC4, 1);
    float fv[4] = { v, 0.0f, 0.0f, 0.0f };
    bgfx_set_uniform(u, fv, 1);
}

void pass_set_vec2(Pass* p, const char* name, vec2 v) {
    (void)p;
    bgfx_uniform_handle_t u = bgfx_create_uniform(name, BGFX_UNIFORM_TYPE_VEC4, 1);
    float fv[4] = { v.x, v.y, 0.0f, 0.0f };
    bgfx_set_uniform(u, fv, 1);
}

void pass_set_vec3(Pass* p, const char* name, vec3 v) {
    (void)p;
    bgfx_uniform_handle_t u = bgfx_create_uniform(name, BGFX_UNIFORM_TYPE_VEC4, 1);
    float fv[4] = { v.x, v.y, v.z, 0.0f };
    bgfx_set_uniform(u, fv, 1);
}

void pass_set_vec4(Pass* p, const char* name, vec4 v) {
    (void)p;
    bgfx_uniform_handle_t u = bgfx_create_uniform(name, BGFX_UNIFORM_TYPE_VEC4, 1);
    float fv[4] = { v.x, v.y, v.z, v.w };
    bgfx_set_uniform(u, fv, 1);
}

void pass_set_mat4(Pass* p, const char* name, mat4 m) {
    (void)p;
    bgfx_uniform_handle_t u = bgfx_create_uniform(name, BGFX_UNIFORM_TYPE_MAT4, 1);
    bgfx_set_uniform(u, m.m, 1);
}

void pass_set_tex(Pass* p, const char* name, int unit) {
    (void)p; (void)name; (void)unit;
    fprintf(stderr,
        "pass_set_tex: use bgfx_set_texture(stage, u, handle, flags) instead "
        "(name=%s unit=%d)\n", name, unit);
}
