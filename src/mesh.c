#include "mesh.h"
#include "asset.h"
#include "texture.h"
#include "cgltf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* CPU triangle data (kept from GL version) */
float* g_mesh_cpu_tris = NULL;
float* g_mesh_cpu_albedos = NULL;
int    g_mesh_cpu_tri_count = 0;

/* One vertex layout for the whole engine. Must match what the shaders
   expect: attribute 0 = position, 1 = normal, 2 = texcoord0, 3 = tangent. */
static bgfx_vertex_layout_t g_layout;
static int g_layout_initialized = 0;

const bgfx_vertex_layout_t* mesh_vertex_layout(void) {
    if (!g_layout_initialized) {
        bgfx_vertex_layout_begin(&g_layout, BGFX_RENDERER_TYPE_COUNT);
        bgfx_vertex_layout_add(&g_layout, BGFX_ATTRIB_POSITION,  3, BGFX_ATTRIB_TYPE_FLOAT, false, false);
        bgfx_vertex_layout_add(&g_layout, BGFX_ATTRIB_NORMAL,    3, BGFX_ATTRIB_TYPE_FLOAT, false, false);
        bgfx_vertex_layout_add(&g_layout, BGFX_ATTRIB_TEXCOORD0, 2, BGFX_ATTRIB_TYPE_FLOAT, false, false);
        bgfx_vertex_layout_add(&g_layout, BGFX_ATTRIB_TANGENT,   4, BGFX_ATTRIB_TYPE_FLOAT, false, false);
        bgfx_vertex_layout_end(&g_layout);
        g_layout_initialized = 1;
    }
    return &g_layout;
}

static bgfx_texture_handle_t mesh_invalid_texture(void) {
    bgfx_texture_handle_t h = { UINT16_MAX };
    return h;
}

static Material default_material(void) {
    Material m = {0};
    m.tex_basecolor = texture_solid(200, 200, 200, 255, 1);
    m.tex_normal    = texture_solid(128, 128, 255, 255, 0);
    m.tex_mr        = texture_solid(255, 200, 0, 255, 0);
    m.tex_emissive  = texture_solid(0, 0, 0, 255, 1);
    m.tex_occlusion = texture_solid(255, 255, 255, 255, 0);
    m.basecolor_factor = v3(1.0f, 1.0f, 1.0f);
    m.metallic_factor  = 0.0f;
    m.roughness_factor = 1.0f;
    m.normal_scale     = 1.0f;
    m.occlusion_strength = 1.0f;
    m.emissive_strength = 1.0f;
    m.emissive_factor   = v3(0.0f, 0.0f, 0.0f);
    return m;
}

static bgfx_texture_handle_t load_texture_from_gltf(cgltf_texture_view* view, int srgb) {
    if (!view || !view->texture) return mesh_invalid_texture();
    cgltf_texture* tex = view->texture;
    if (!tex->image) return mesh_invalid_texture();

    const cgltf_buffer_view* bv = tex->image->buffer_view;
    if (!bv) {
        fprintf(stderr, "texture: image has no buffer_view\n");
        return mesh_invalid_texture();
    }
    const unsigned char* base = (const unsigned char*)bv->buffer->data;
    if (!base) return mesh_invalid_texture();
    const unsigned char* bytes = base + bv->offset;

    return texture_from_memory(bytes, (int)bv->size, srgb);
}

static Material material_from_gltf(cgltf_material* gm) {
    Material m = default_material();
    if (!gm) return m;

    if (gm->has_pbr_metallic_roughness) {
        cgltf_pbr_metallic_roughness* p = &gm->pbr_metallic_roughness;
        m.basecolor_factor = v3(p->base_color_factor[0], p->base_color_factor[1], p->base_color_factor[2]);
        m.metallic_factor  = p->metallic_factor;
        m.roughness_factor = p->roughness_factor;

        bgfx_texture_handle_t bc = load_texture_from_gltf(&p->base_color_texture, 1);
        if (bc.idx != UINT16_MAX) { m.tex_basecolor = bc; m.has_basecolor = 1; }

        bgfx_texture_handle_t mr = load_texture_from_gltf(&p->metallic_roughness_texture, 0);
        if (mr.idx != UINT16_MAX) { m.tex_mr = mr; m.has_mr = 1; }
    }

    bgfx_texture_handle_t nm = load_texture_from_gltf(&gm->normal_texture, 0);
    if (nm.idx != UINT16_MAX) { m.tex_normal = nm; m.has_normal = 1; m.normal_scale = gm->normal_texture.scale; }

    m.emissive_factor = v3(gm->emissive_factor[0], gm->emissive_factor[1], gm->emissive_factor[2]);
    if (gm->has_emissive_strength) {
        m.emissive_strength = gm->emissive_strength.emissive_strength;
    }

    bgfx_texture_handle_t em = load_texture_from_gltf(&gm->emissive_texture, 1);
    if (em.idx != UINT16_MAX) { m.tex_emissive = em; m.has_emissive = 1; }

    if (m.emissive_factor.x > 0.0f || m.emissive_factor.y > 0.0f ||
        m.emissive_factor.z > 0.0f || m.emissive_strength > 1.0f) {
        m.has_emissive = 1;
    }

    bgfx_texture_handle_t oc = load_texture_from_gltf(&gm->occlusion_texture, 0);
    if (oc.idx != UINT16_MAX) { m.tex_occlusion = oc; m.has_occlusion = 1; }

    m.double_sided = gm->double_sided ? 1 : 0;
    return m;
}

/* Reuse the accessor_to_float helper from the GL version (unchanged). */
static float* accessor_to_float(cgltf_accessor* acc, int components) {
    if (!acc) return NULL;
    float* out = malloc(sizeof(float) * acc->count * components);
    if (!out) return NULL;
    for (cgltf_size i = 0; i < acc->count; ++i) {
        cgltf_accessor_read_float(acc, i, out + i * components, components);
    }
    return out;
}

/* Material signature for merging (same as GL version). */
typedef struct {
    uint16_t tb, tn, tm, te, to;
    float bc[3];
    float metal, rough;
    int hbc, hmr, hnm;
} MatSig;

static MatSig mat_signature(const Material* m) {
    MatSig s;
    memset(&s, 0, sizeof(s));
    s.tb = m->tex_basecolor.idx;
    s.tn = m->tex_normal.idx;
    s.tm = m->tex_mr.idx;
    s.te = m->tex_emissive.idx;
    s.to = m->tex_occlusion.idx;
    s.bc[0] = m->basecolor_factor.x;
    s.bc[1] = m->basecolor_factor.y;
    s.bc[2] = m->basecolor_factor.z;
    s.metal  = m->metallic_factor;
    s.rough  = m->roughness_factor;
    s.hbc = m->has_basecolor;
    s.hmr = m->has_mr;
    s.hnm = m->has_normal;
    return s;
}

static int mat_equal(const MatSig* a, const MatSig* b) {
    if (a->tb != b->tb) return 0;
    if (a->tn != b->tn) return 0;
    if (a->tm != b->tm) return 0;
    if (a->te != b->te) return 0;
    if (a->to != b->to) return 0;
    if (a->hbc != b->hbc || a->hmr != b->hmr || a->hnm != b->hnm) return 0;
    for (int i = 0; i < 3; ++i) if (fabsf(a->bc[i] - b->bc[i]) > 1e-4f) return 0;
    if (fabsf(a->metal - b->metal) > 1e-4f) return 0;
    if (fabsf(a->rough - b->rough) > 1e-4f) return 0;
    return 1;
}

Model model_load_glb(const char* path) {
    Model model = {0};
    model.min_bb = v3( 1e30f,  1e30f,  1e30f);
    model.max_bb = v3(-1e30f, -1e30f, -1e30f);

    size_t file_len = 0;
    char* file_data = asset_load(path, &file_len);
    if (!file_data) {
        fprintf(stderr, "mesh: cannot load %s\n", path);
        return model;
    }

    cgltf_options opts = {0};
    cgltf_data* data = NULL;
    cgltf_result r = cgltf_parse(&opts, file_data, file_len, &data);
    if (r != cgltf_result_success) {
        fprintf(stderr, "mesh: cgltf_parse failed (%d)\n", r);
        free(file_data);
        return model;
    }
    r = cgltf_load_buffers(&opts, data, NULL);
    if (r != cgltf_result_success) {
        fprintf(stderr, "mesh: cgltf_load_buffers failed\n");
        cgltf_free(data); free(file_data);
        return model;
    }

    printf("mesh: loaded %s\n", path);

    /* CPU triangle accumulator (unchanged from GL version) */
    static float* g_cpu_tris = NULL;
    static float* g_cpu_albedo = NULL;
    static int    g_cpu_tri_count = 0;
    static int    g_cpu_tri_cap = 0;

    #define CPU_TRI_ADD(px0,py0,pz0,px1,py1,pz1,px2,py2,pz2, ar,ag,ab) do { \
        if (g_cpu_tri_count >= g_cpu_tri_cap) { \
            g_cpu_tri_cap = g_cpu_tri_cap ? g_cpu_tri_cap * 2 : 4096; \
            g_cpu_tris = realloc(g_cpu_tris, sizeof(float) * g_cpu_tri_cap * 9); \
            g_cpu_albedo = realloc(g_cpu_albedo, sizeof(float) * g_cpu_tri_cap * 3); \
        } \
        float* tp = g_cpu_tris + g_cpu_tri_count * 9; \
        tp[0]=(px0); tp[1]=(py0); tp[2]=(pz0); \
        tp[3]=(px1); tp[4]=(py1); tp[5]=(pz1); \
        tp[6]=(px2); tp[7]=(py2); tp[8]=(pz2); \
        float* ap = g_cpu_albedo + g_cpu_tri_count * 3; \
        ap[0]=(ar); ap[1]=(ag); ap[2]=(ab); \
        g_cpu_tri_count++; \
    } while(0)

    /* Group by material (unchanged from GL version) */
    typedef struct {
        int          mat_index;
        float*       verts;
        size_t       vert_count;
        size_t       vert_capacity;
        uint32_t*    indices;
        size_t       index_count;
        size_t       index_capacity;
    } Group;

    Group* groups = NULL;
    int    group_count = 0;
    Material* group_mats = NULL;

    for (size_t mi = 0; mi < data->meshes_count; ++mi) {
        cgltf_mesh* gm = &data->meshes[mi];
        for (size_t pi = 0; pi < gm->primitives_count; ++pi) {
            cgltf_primitive* prim = &gm->primitives[pi];
            if (prim->type != cgltf_primitive_type_triangles) continue;

            cgltf_accessor *a_pos=NULL, *a_nrm=NULL, *a_uv=NULL, *a_tan=NULL;
            for (size_t ai = 0; ai < prim->attributes_count; ++ai) {
                cgltf_attribute* at = &prim->attributes[ai];
                if (at->type == cgltf_attribute_type_position)  a_pos = at->data;
                if (at->type == cgltf_attribute_type_normal)    a_nrm = at->data;
                if (at->type == cgltf_attribute_type_texcoord && at->index == 0) a_uv = at->data;
                if (at->type == cgltf_attribute_type_tangent)   a_tan = at->data;
            }
            if (!a_pos) continue;

            size_t vcount = a_pos->count;
            float* pos = accessor_to_float(a_pos, 3);
            float* nrm = a_nrm ? accessor_to_float(a_nrm, 3) : NULL;
            float* uv  = a_uv  ? accessor_to_float(a_uv, 2)  : NULL;
            float* tan = a_tan ? accessor_to_float(a_tan, 4) : NULL;

            Material pm = prim->material ? material_from_gltf(prim->material) : default_material();
            MatSig sig = mat_signature(&pm);

            int g = -1;
            for (int k = 0; k < group_count; ++k) {
                Material* km = &group_mats[k];
                MatSig ks = mat_signature(km);
                if (mat_equal(&ks, &sig)) { g = k; break; }
            }
            if (g < 0) {
                groups = realloc(groups, sizeof(Group) * (group_count + 1));
                group_mats = realloc(group_mats, sizeof(Material) * (group_count + 1));
                memset(&groups[group_count], 0, sizeof(Group));
                groups[group_count].mat_index = group_count;
                group_mats[group_count] = pm;
                g = group_count;
                group_count++;
            }

            size_t needed = groups[g].vert_count + vcount;
            if (needed > groups[g].vert_capacity) {
                size_t new_cap = needed * 2;
                groups[g].verts = realloc(groups[g].verts, sizeof(float) * 12 * new_cap);
                groups[g].vert_capacity = new_cap;
            }
            uint32_t base_vertex = (uint32_t)groups[g].vert_count;
            float* dst = groups[g].verts + groups[g].vert_count * 12;
            for (size_t v = 0; v < vcount; ++v) {
                dst[v*12+0] = pos[v*3+0];
                dst[v*12+1] = pos[v*3+1];
                dst[v*12+2] = pos[v*3+2];
                if (nrm) { dst[v*12+3] = nrm[v*3+0]; dst[v*12+4] = nrm[v*3+1]; dst[v*12+5] = nrm[v*3+2]; }
                else     { dst[v*12+3]=0; dst[v*12+4]=0; dst[v*12+5]=1; }
                if (uv)  { dst[v*12+6] = uv[v*2+0]; dst[v*12+7] = uv[v*2+1]; }
                else     { dst[v*12+6]=0; dst[v*12+7]=0; }
                if (tan) { dst[v*12+8] = tan[v*4+0]; dst[v*12+9] = tan[v*4+1]; dst[v*12+10] = tan[v*4+2]; dst[v*12+11] = tan[v*4+3]; }
                else     { dst[v*12+8]=1; dst[v*12+9]=0; dst[v*12+10]=0; dst[v*12+11]=1; }

                float x=dst[v*12+0], y=dst[v*12+1], z=dst[v*12+2];
                if (x < model.min_bb.x) model.min_bb.x = x;
                if (y < model.min_bb.y) model.min_bb.y = y;
                if (z < model.min_bb.z) model.min_bb.z = z;
                if (x > model.max_bb.x) model.max_bb.x = x;
                if (y > model.max_bb.y) model.max_bb.y = y;
                if (z > model.max_bb.z) model.max_bb.z = z;
            }
            groups[g].vert_count += vcount;

            size_t icount = prim->indices ? prim->indices->count : vcount;
            if (groups[g].index_count + icount > groups[g].index_capacity) {
                size_t new_cap = (groups[g].index_count + icount) * 2;
                groups[g].indices = realloc(groups[g].indices, sizeof(uint32_t) * new_cap);
                groups[g].index_capacity = new_cap;
            }
            uint32_t* idst = groups[g].indices + groups[g].index_count;
            if (prim->indices) {
                for (size_t i = 0; i < icount; ++i) {
                    idst[i] = (uint32_t)cgltf_accessor_read_index(prim->indices, i) + base_vertex;
                }
            } else {
                for (size_t i = 0; i < icount; ++i) idst[i] = base_vertex + (uint32_t)i;
            }
            groups[g].index_count += icount;

            /* Emit triangles for SG bake data */
            {
                float ar = pm.basecolor_factor.x;
                float ag = pm.basecolor_factor.y;
                float ab = pm.basecolor_factor.z;
                size_t tri_n = icount / 3;
                for (size_t ti = 0; ti < tri_n; ++ti) {
                    uint32_t i0, i1, i2;
                    if (prim->indices) {
                        i0 = (uint32_t)cgltf_accessor_read_index(prim->indices, ti*3+0);
                        i1 = (uint32_t)cgltf_accessor_read_index(prim->indices, ti*3+1);
                        i2 = (uint32_t)cgltf_accessor_read_index(prim->indices, ti*3+2);
                    } else {
                        i0 = (uint32_t)(ti*3+0);
                        i1 = (uint32_t)(ti*3+1);
                        i2 = (uint32_t)(ti*3+2);
                    }
                    CPU_TRI_ADD(dst[i0*12+0], dst[i0*12+1], dst[i0*12+2],
                                dst[i1*12+0], dst[i1*12+1], dst[i1*12+2],
                                dst[i2*12+0], dst[i2*12+1], dst[i2*12+2],
                                ar, ag, ab);
                }
            }

            free(pos); free(nrm); free(uv); free(tan);
        }
    }

    /* Create bgfx buffers per group */
    model.meshes = calloc(group_count, sizeof(Mesh));
    model.count = group_count;
    const bgfx_vertex_layout_t* layout = mesh_vertex_layout();

    for (int g = 0; g < group_count; ++g) {
        Mesh* out = &model.meshes[g];
        out->material = group_mats[g];
        out->model = m4_identity();

        uint32_t max_idx = 0;
        for (size_t i = 0; i < groups[g].index_count; ++i) {
            if (groups[g].indices[i] > max_idx) max_idx = groups[g].indices[i];
        }
        int use_16 = (max_idx < 65535);

        size_t vert_bytes = sizeof(float) * 12 * groups[g].vert_count;
        const bgfx_memory_t* vmem = bgfx_copy(groups[g].verts, (uint32_t)vert_bytes);
        out->vb = bgfx_create_vertex_buffer(vmem, layout, BGFX_BUFFER_NONE);

        if (use_16) {
            uint16_t* idx16 = malloc(sizeof(uint16_t) * groups[g].index_count);
            for (size_t i = 0; i < groups[g].index_count; ++i) idx16[i] = (uint16_t)groups[g].indices[i];
            const bgfx_memory_t* imem = bgfx_copy(idx16, (uint32_t)(sizeof(uint16_t) * groups[g].index_count));
            out->ib = bgfx_create_index_buffer(imem, BGFX_BUFFER_INDEX32 & 0);
            free(idx16);
        } else {
            const bgfx_memory_t* imem = bgfx_copy(groups[g].indices, (uint32_t)(sizeof(uint32_t) * groups[g].index_count));
            out->ib = bgfx_create_index_buffer(imem, BGFX_BUFFER_INDEX32);
        }
        out->index_count = (uint32_t)groups[g].index_count;
        out->use_16bit_indices = use_16;

        free(groups[g].verts);
        free(groups[g].indices);
    }
    free(groups);
    free(group_mats);

    g_mesh_cpu_tris = g_cpu_tris;
    g_mesh_cpu_albedos = g_cpu_albedo;
    g_mesh_cpu_tri_count = g_cpu_tri_count;

    cgltf_free(data);
    free(file_data);
    return model;
}

void model_destroy(Model* m) {
    if (!m->meshes) return;
    for (int i = 0; i < m->count; ++i) {
        Mesh* mesh = &m->meshes[i];
        if (mesh->vb.idx != UINT16_MAX) bgfx_destroy_vertex_buffer(mesh->vb);
        if (mesh->ib.idx != UINT16_MAX) bgfx_destroy_index_buffer(mesh->ib);
    }
    free(m->meshes);
    m->meshes = NULL;
    m->count = 0;
}

vec3 model_center(Model* m) {
    return v3((m->min_bb.x + m->max_bb.x) * 0.5f,
              (m->min_bb.y + m->max_bb.y) * 0.5f,
              (m->min_bb.z + m->max_bb.z) * 0.5f);
}

vec3 model_extent(Model* m) {
    return v3(m->max_bb.x - m->min_bb.x,
              m->max_bb.y - m->min_bb.y,
              m->max_bb.z - m->min_bb.z);
}
