#ifndef MESH_H
#define MESH_H

#include <bgfx/c99/bgfx.h>
#include "math3d.h"

typedef struct {
    bgfx_texture_handle_t tex_basecolor;
    bgfx_texture_handle_t tex_normal;
    bgfx_texture_handle_t tex_mr;
    bgfx_texture_handle_t tex_emissive;
    bgfx_texture_handle_t tex_occlusion;

    vec3  basecolor_factor;
    float metallic_factor;
    float roughness_factor;
    float normal_scale;
    float occlusion_strength;
    vec3  emissive_factor;
    float emissive_strength;

    int has_basecolor;
    int has_normal;
    int has_mr;
    int has_emissive;
    int has_occlusion;
    int double_sided;
} Material;

typedef struct {
    bgfx_vertex_buffer_handle_t vb;
    bgfx_index_buffer_handle_t  ib;
    uint32_t index_count;
    int      use_16bit_indices;
    Material material;
    mat4     model;
} Mesh;

typedef struct {
    Mesh*   meshes;
    int     count;
    vec3    min_bb;
    vec3    max_bb;
} Model;

Model model_load_glb(const char* path);
void  model_destroy(Model* m);

vec3 model_center(Model* m);
vec3 model_extent(Model* m);

extern float* g_mesh_cpu_tris;
extern float* g_mesh_cpu_albedos;
extern int    g_mesh_cpu_tri_count;

/* The bgfx VertexLayout used for all engine meshes.
   Position (vec3), Normal (vec3), UV (vec2), Tangent (vec4). */
const bgfx_vertex_layout_t* mesh_vertex_layout(void);

#endif
