/* bgfx port of the engine's main loop.
   Runs alongside main.c (WebGL build); built by Makefile.bgfx. */

#include <bgfx/c99/bgfx.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__linux__)
#  define GLFW_EXPOSE_NATIVE_X11
#  define GLFW_EXPOSE_NATIVE_GLX
#  include <GLFW/glfw3native.h>
#endif

#include "math3d.h"
#include "pass.h"
#include "framebuffer.h"
#include "texture.h"
#include "hdr_env.h"
#include "mesh.h"
#include "game.h"

#define W 1280
#define H 720

/* bgfx view IDs. bgfx executes views in ascending order unless you
   set a custom order. Keep these in pass order. */
enum {
    VIEW_SHADOW   = 1,
    VIEW_SCENE    = 2,
    VIEW_SKY      = 3,
    VIEW_TONEMAP  = 4,
};

/* ---- Global engine state ---- */
static GLFWwindow*   g_window      = NULL;
static Framebuffer   g_scene_fb;
static Framebuffer   g_shadow_fb;
static HDREnv        g_hdr_env;
static Model         g_model;
static mat4          g_model_base = {{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}};

static Pass          g_pbr_pass;
static Pass          g_sky_pass;
static Pass          g_shadow_pass;
static Pass          g_tonemap_pass;

/* Uniform handles for the pbr pass (created once) */
static bgfx_uniform_handle_t u_modelMat;
static bgfx_uniform_handle_t u_engineViewProj;
static bgfx_uniform_handle_t u_lightVPMat;
static bgfx_uniform_handle_t u_normalMat;
static bgfx_uniform_handle_t u_camPosEngine;
static bgfx_uniform_handle_t u_lightDirEngine;
static bgfx_uniform_handle_t u_lightColorEngine;
static bgfx_uniform_handle_t u_lightIntensityEngine;
static bgfx_uniform_handle_t u_debugMode;
static bgfx_uniform_handle_t u_sceneGridMin;
static bgfx_uniform_handle_t u_sceneGridMax;
static bgfx_uniform_handle_t u_resolution;
static bgfx_uniform_handle_t u_basecolorFactor;
static bgfx_uniform_handle_t u_metallicFactor;
static bgfx_uniform_handle_t u_roughnessFactor;
static bgfx_uniform_handle_t u_normalScale;
static bgfx_uniform_handle_t u_hasEmissive;
static bgfx_uniform_handle_t u_emissiveFactor;
static bgfx_uniform_handle_t u_emissiveStrength;
static bgfx_uniform_handle_t u_hasBasecolor;
static bgfx_uniform_handle_t u_hasMR;
static bgfx_uniform_handle_t u_hasNormal;
static bgfx_uniform_handle_t u_shadowEnabled;

static bgfx_uniform_handle_t s_sh;
static bgfx_uniform_handle_t s_probeSGs;
static bgfx_uniform_handle_t s_ssrColor;
static bgfx_uniform_handle_t s_ssao;
static bgfx_uniform_handle_t s_gbufferAlbedo;
static bgfx_uniform_handle_t s_ssgi;
static bgfx_uniform_handle_t s_ssrDepth;
static bgfx_uniform_handle_t s_texBasecolor;
static bgfx_uniform_handle_t s_texMR;
static bgfx_uniform_handle_t s_texNormal;
static bgfx_uniform_handle_t s_texEmissive;
static bgfx_uniform_handle_t s_texOcclusion;
static bgfx_uniform_handle_t s_shadowMap;

/* Sky pass uniforms */
static bgfx_uniform_handle_t sky_u_invViewProj;
static bgfx_uniform_handle_t sky_u_invResolution;
static bgfx_uniform_handle_t sky_u_hdriValid;
static bgfx_uniform_handle_t sky_s_hdri;

/* Shadow pass uniforms */
static bgfx_uniform_handle_t sh_u_lightVP;
static bgfx_uniform_handle_t sh_u_modelMat;

/* Tonemap pass uniforms */
static bgfx_uniform_handle_t tm_s_scene;
static bgfx_uniform_handle_t tm_u_invSize;

/* Camera state (world units) */
static mat4 g_view;
static mat4 g_proj;
static mat4 g_viewproj;
static mat4 g_light_viewproj;
static vec3 g_cam_pos = { 0, 0, 0 };

static float g_time = 0.0f;
static float g_last_time = 0.0f;

/* ------------------------------------------------------------------ */
/* Init                                                               */
/* ------------------------------------------------------------------ */

static void init_window(void) {
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    if (!glfwInit()) { fprintf(stderr, "glfwInit failed\n"); exit(1); }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    g_window = glfwCreateWindow(W, H, "Afterburn (bgfx)", NULL, NULL);
    if (!g_window) { fprintf(stderr, "create window failed\n"); exit(1); }
}

static void init_bgfx(void) {
    bgfx_init_t init;
    bgfx_init_ctor(&init);
    init.type = BGFX_RENDERER_TYPE_OPENGL;
    init.platformData.type = BGFX_NATIVE_WINDOW_HANDLE_TYPE_DEFAULT;
    init.swapChain.nwh    = (void*)(uintptr_t)glfwGetX11Window(g_window);
    init.swapChain.ndt    = glfwGetX11Display();
    init.swapChain.width  = W;
    init.swapChain.height = H;
    init.swapChain.flags  = BGFX_SWAP_CHAIN_NONE;
    init.swapChain.formatColor        = BGFX_TEXTURE_FORMAT_BGRA8;
    init.swapChain.formatDepthStencil = BGFX_TEXTURE_FORMAT_D24S8;

    if (!bgfx_init(&init)) {
        fprintf(stderr, "bgfx_init failed\n");
        exit(1);
    }

    {
        bgfx_frame_buffer_handle_t no_fb = { UINT16_MAX };
        bgfx_set_debug(BGFX_DEBUG_TEXT, no_fb, 0);
    }
    bgfx_set_view_clear(0, BGFX_CLEAR_COLOR, 0x00000000, 1.0f, 0);

}

static void init_framebuffers(void) {
    g_scene_fb = fb_create(W, H, FB_RGBA16F);
    if (!g_scene_fb.valid) { fprintf(stderr, "scene_fb creation failed\n"); exit(1); }

    g_shadow_fb = fb_create(2048, 2048, FB_R8);
    if (!g_shadow_fb.valid) { fprintf(stderr, "shadow_fb creation failed\n"); exit(1); }
}

static void init_passes(void) {
    g_sky_pass = pass_create_from_bin(
        "shaders/out/gl/fullscreen.vert.bin",
        "shaders/out/gl/sky_background.frag.bin");
    g_pbr_pass = pass_create_from_bin(
        "shaders/out/gl/pbr.vert.bin",
        "shaders/out/gl/pbr.frag.bin");
    g_shadow_pass = pass_create_from_bin(
        "shaders/out/gl/shadow_depth.vert.bin",
        "shaders/out/gl/shadow_depth.frag.bin");
    g_tonemap_pass = pass_create_from_bin(
        "shaders/out/gl/fullscreen.vert.bin",
        "shaders/out/gl/blit_tonemap.frag.bin");

    if (!g_sky_pass.valid || !g_pbr_pass.valid || !g_shadow_pass.valid || !g_tonemap_pass.valid) {
        fprintf(stderr, "pass creation failed\n");
        exit(1);
    }
}

static void init_uniforms(void) {
    /* PBR */
    u_modelMat             = bgfx_create_uniform("u_modelMat",             BGFX_UNIFORM_TYPE_MAT4, 1);
    u_engineViewProj       = bgfx_create_uniform("u_engineViewProj",       BGFX_UNIFORM_TYPE_MAT4, 1);
    u_lightVPMat           = bgfx_create_uniform("u_lightVPMat",           BGFX_UNIFORM_TYPE_MAT4, 1);
    u_normalMat            = bgfx_create_uniform("u_normalMat",            BGFX_UNIFORM_TYPE_MAT4, 1);
    u_camPosEngine         = bgfx_create_uniform("u_camPosEngine",         BGFX_UNIFORM_TYPE_VEC4, 1);
    u_lightDirEngine       = bgfx_create_uniform("u_lightDirEngine",       BGFX_UNIFORM_TYPE_VEC4, 1);
    u_lightColorEngine     = bgfx_create_uniform("u_lightColorEngine",     BGFX_UNIFORM_TYPE_VEC4, 1);
    u_lightIntensityEngine = bgfx_create_uniform("u_lightIntensityEngine", BGFX_UNIFORM_TYPE_VEC4, 1);
    u_debugMode            = bgfx_create_uniform("u_debugMode",            BGFX_UNIFORM_TYPE_VEC4, 1);
    u_sceneGridMin         = bgfx_create_uniform("u_sceneGridMin",         BGFX_UNIFORM_TYPE_VEC4, 1);
    u_sceneGridMax         = bgfx_create_uniform("u_sceneGridMax",         BGFX_UNIFORM_TYPE_VEC4, 1);
    u_resolution           = bgfx_create_uniform("u_resolution",           BGFX_UNIFORM_TYPE_VEC4, 1);
    u_basecolorFactor      = bgfx_create_uniform("u_basecolorFactor",      BGFX_UNIFORM_TYPE_VEC4, 1);
    u_metallicFactor       = bgfx_create_uniform("u_metallicFactor",       BGFX_UNIFORM_TYPE_VEC4, 1);
    u_roughnessFactor      = bgfx_create_uniform("u_roughnessFactor",      BGFX_UNIFORM_TYPE_VEC4, 1);
    u_normalScale          = bgfx_create_uniform("u_normalScale",          BGFX_UNIFORM_TYPE_VEC4, 1);
    u_hasEmissive          = bgfx_create_uniform("u_hasEmissive",          BGFX_UNIFORM_TYPE_VEC4, 1);
    u_emissiveFactor       = bgfx_create_uniform("u_emissiveFactor",       BGFX_UNIFORM_TYPE_VEC4, 1);
    u_emissiveStrength     = bgfx_create_uniform("u_emissiveStrength",     BGFX_UNIFORM_TYPE_VEC4, 1);
    u_hasBasecolor         = bgfx_create_uniform("u_hasBasecolor",         BGFX_UNIFORM_TYPE_VEC4, 1);
    u_hasMR                = bgfx_create_uniform("u_hasMR",                BGFX_UNIFORM_TYPE_VEC4, 1);
    u_hasNormal            = bgfx_create_uniform("u_hasNormal",            BGFX_UNIFORM_TYPE_VEC4, 1);
    u_shadowEnabled        = bgfx_create_uniform("u_shadowEnabled",        BGFX_UNIFORM_TYPE_VEC4, 1);

    /* PBR samplers */
    s_sh             = bgfx_create_uniform("s_sh",             BGFX_UNIFORM_TYPE_SAMPLER, 1);
    s_probeSGs       = bgfx_create_uniform("s_probeSGs",       BGFX_UNIFORM_TYPE_SAMPLER, 1);
    s_ssrColor       = bgfx_create_uniform("s_ssrColor",       BGFX_UNIFORM_TYPE_SAMPLER, 1);
    s_ssao           = bgfx_create_uniform("s_ssao",           BGFX_UNIFORM_TYPE_SAMPLER, 1);
    s_gbufferAlbedo  = bgfx_create_uniform("s_gbufferAlbedo",  BGFX_UNIFORM_TYPE_SAMPLER, 1);
    s_ssgi           = bgfx_create_uniform("s_ssgi",           BGFX_UNIFORM_TYPE_SAMPLER, 1);
    s_ssrDepth       = bgfx_create_uniform("s_ssrDepth",       BGFX_UNIFORM_TYPE_SAMPLER, 1);
    s_texBasecolor   = bgfx_create_uniform("s_texBasecolor",   BGFX_UNIFORM_TYPE_SAMPLER, 1);
    s_texMR          = bgfx_create_uniform("s_texMR",          BGFX_UNIFORM_TYPE_SAMPLER, 1);
    s_texNormal      = bgfx_create_uniform("s_texNormal",      BGFX_UNIFORM_TYPE_SAMPLER, 1);
    s_texEmissive    = bgfx_create_uniform("s_texEmissive",    BGFX_UNIFORM_TYPE_SAMPLER, 1);
    s_texOcclusion   = bgfx_create_uniform("s_texOcclusion",   BGFX_UNIFORM_TYPE_SAMPLER, 1);
    s_shadowMap      = bgfx_create_uniform("s_shadowMap",      BGFX_UNIFORM_TYPE_SAMPLER, 1);

    /* Sky */
    sky_u_invViewProj = bgfx_create_uniform("u_customInvViewProj", BGFX_UNIFORM_TYPE_MAT4,    1);
    sky_u_invResolution = bgfx_create_uniform("u_invResolution",   BGFX_UNIFORM_TYPE_VEC4,    1);
    sky_u_hdriValid   = bgfx_create_uniform("u_hdriValid",         BGFX_UNIFORM_TYPE_VEC4,    1);
    sky_s_hdri        = bgfx_create_uniform("s_hdri",              BGFX_UNIFORM_TYPE_SAMPLER, 1);

    /* Shadow */
    sh_u_lightVP = bgfx_create_uniform("u_lightVPMat", BGFX_UNIFORM_TYPE_MAT4, 1);
    sh_u_modelMat = bgfx_create_uniform("u_modelMat",  BGFX_UNIFORM_TYPE_MAT4, 1);

    /* Tonemap */
    tm_s_scene  = bgfx_create_uniform("s_scene",   BGFX_UNIFORM_TYPE_SAMPLER, 1);
    tm_u_invSize = bgfx_create_uniform("u_invSize", BGFX_UNIFORM_TYPE_VEC4,    1);
}

static void init_camera(void) {
    /* Fixed chase camera for now. game.c will supply the real one
       when we wire it in. */
    vec3 eye    = v3(0.0f, 2.0f, 6.0f);
    vec3 target = v3(0.0f, 0.5f, 0.0f);
    vec3 up     = v3(0.0f, 1.0f, 0.0f);
    g_view = m4_look_at(eye, target, up);
    g_proj = m4_perspective(50.0f * 3.14159265f / 180.0f,
                            (float)W / (float)H, 0.1f, 200.0f);
    g_viewproj = m4_mul(g_proj, g_view);
    g_cam_pos = eye;

    /* Shadow light matrix — ortho looking down the light direction. */
    vec3 light_dir = v3_norm(v3(-0.4f, -1.0f, -0.5f));
    vec3 light_pos = v3_scale(light_dir, -20.0f);
    mat4 light_view = m4_look_at(light_pos, v3(0,0,0), v3(0,1,0));
    /* Orthographic box, roughly the model's bounds. */
    mat4 light_proj;
    {
        float l = -6.0f, r = 6.0f, b = -6.0f, t = 6.0f, n = 0.1f, f = 50.0f;
        memset(light_proj.m, 0, sizeof(light_proj.m));
        light_proj.m[0]  =  2.0f / (r - l);
        light_proj.m[5]  =  2.0f / (t - b);
        light_proj.m[10] = -2.0f / (f - n);
        light_proj.m[12] = -(r + l) / (r - l);
        light_proj.m[13] = -(t + b) / (t - b);
        light_proj.m[14] = -(f + n) / (f - n);
        light_proj.m[15] =  1.0f;
    }
    g_light_viewproj = m4_mul(light_proj, light_view);
}

/* ------------------------------------------------------------------ */
/* Draw helpers                                                       */
/* ------------------------------------------------------------------ */

static void draw_model_shadow(void) {
    float light_vp[16];
    memcpy(light_vp, g_light_viewproj.m, sizeof(light_vp));
    bgfx_set_uniform(sh_u_lightVP, light_vp, 1);

    for (int i = 0; i < g_model.count; ++i) {
        Mesh* m = &g_model.meshes[i];
        bgfx_set_uniform(sh_u_modelMat, m->model.m, 1);
        bgfx_set_vertex_buffer(0, m->vb, 0, UINT32_MAX);
        bgfx_set_index_buffer(m->ib, 0, UINT32_MAX);
        bgfx_set_state(BGFX_STATE_WRITE_Z
                     | BGFX_STATE_DEPTH_TEST_LESS
                     | BGFX_STATE_CULL_CCW, 0);
        bgfx_submit(VIEW_SHADOW, g_shadow_pass.prog, 0, false);
    }
}

static void draw_model_pbr(void) {
    static int dbg = 0;
    if (dbg++ % 60 == 0) {
        mat4 jx = game_jet_transform();
        printf("[draw] jet pos=(%.2f,%.2f,%.2f)  cam=(%.2f,%.2f,%.2f)  meshes=%d  model_base_pos=(%.2f,%.2f,%.2f)\n",
               jx.m[12], jx.m[13], jx.m[14],
               g_cam_pos.x, g_cam_pos.y, g_cam_pos.z,
               g_model.count,
               g_model_base.m[12], g_model_base.m[13], g_model_base.m[14]);
    }

    /* Camera / light uniforms */
    float cam[4]       = { g_cam_pos.x, g_cam_pos.y, g_cam_pos.z, 0 };
    float light_dir[4] = { -0.4f, -1.0f, -0.5f, 0 };  /* direction light travels */
    float light_col[4] = { 1.0f, 0.98f, 0.95f, 0 };
    float light_int[4] = { 3.0f, 0, 0, 0 };
    float resolution[4] = { (float)W, (float)H, 0, 0 };
    float debug[4]     = { 7, 0, 0, 0 };
    float grid_min[4]  = { -6, -1, -6, 0 };
    float grid_max[4]  = {  6,  5,  6, 0 };
    float shadow_en[4] = { 0, 0, 0, 0 };  /* disable shadow lookup until it works */

    bgfx_set_uniform(u_camPosEngine,         cam,       1);
    bgfx_set_uniform(u_lightDirEngine,       light_dir, 1);
    bgfx_set_uniform(u_lightColorEngine,     light_col, 1);
    bgfx_set_uniform(u_lightIntensityEngine, light_int, 1);
    bgfx_set_uniform(u_debugMode,            debug,     1);
    bgfx_set_uniform(u_sceneGridMin,         grid_min,  1);
    bgfx_set_uniform(u_sceneGridMax,         grid_max,  1);
    bgfx_set_uniform(u_resolution,           resolution,1);
    bgfx_set_uniform(u_engineViewProj,       g_viewproj.m, 1);
    bgfx_set_uniform(u_lightVPMat,           g_light_viewproj.m, 1);
    bgfx_set_uniform(u_shadowEnabled,        shadow_en, 1);

    /* Bind "empty" textures for the passes we haven't wired yet.
       These must be valid handles or bgfx will complain. */
    bgfx_texture_handle_t empty = g_scene_fb.color;
    bgfx_set_texture(0, s_sh,            empty, UINT32_MAX);
    bgfx_set_texture(1, s_probeSGs,      empty, UINT32_MAX);
    bgfx_set_texture(2, s_ssrColor,      empty, UINT32_MAX);
    bgfx_set_texture(3, s_ssao,          empty, UINT32_MAX);
    bgfx_set_texture(4, s_gbufferAlbedo, empty, UINT32_MAX);
    bgfx_set_texture(5, s_ssgi,          empty, UINT32_MAX);
    bgfx_set_texture(6, s_ssrDepth,      empty, UINT32_MAX);
    bgfx_set_texture(12, s_shadowMap,    empty, UINT32_MAX);

    static int dbg_mesh = 0;
    if (dbg_mesh++ == 1) {
        for (int i = 0; i < g_model.count; ++i) {
            Mesh* m = &g_model.meshes[i];
            printf("[mesh] %d: vb=%u ib=%u idx=%u 16bit=%d\n",
                   i, m->vb.idx, m->ib.idx, m->index_count, m->use_16bit_indices);
        }
    }
    /* The jet's world transform from game.c. */
    mat4 jet_xform = game_jet_transform();

    for (int i = 0; i < g_model.count; ++i) {
        Mesh* m = &g_model.meshes[i];

        /* Combine the jet's world transform with the model's local transform. */
        mat4 world = m4_mul(jet_xform, m4_mul(g_model_base, m->model));

        float normal_mat[9];
        m4_to_mat3_normal(world, normal_mat);
        bgfx_set_uniform(u_modelMat,   world.m,      1);
        bgfx_set_uniform(u_normalMat,  normal_mat,   1);

        float base[4]  = { m->material.basecolor_factor.x, m->material.basecolor_factor.y, m->material.basecolor_factor.z, 0 };
        float metal[4] = { m->material.metallic_factor, 0, 0, 0 };
        float rough[4] = { m->material.roughness_factor, 0, 0, 0 };
        float nscale[4]= { m->material.normal_scale, 0, 0, 0 };
        float emfac[4] = { m->material.emissive_factor.x, m->material.emissive_factor.y, m->material.emissive_factor.z, 0 };
        float emstr[4] = { m->material.emissive_strength, 0, 0, 0 };
        float hbc[4]   = { (float)m->material.has_basecolor, 0, 0, 0 };
        float hmr[4]   = { (float)m->material.has_mr, 0, 0, 0 };
        float hnm[4]   = { (float)m->material.has_normal, 0, 0, 0 };
        float hem[4]   = { (float)m->material.has_emissive, 0, 0, 0 };

        bgfx_set_uniform(u_basecolorFactor,  base,   1);
        bgfx_set_uniform(u_metallicFactor,   metal,  1);
        bgfx_set_uniform(u_roughnessFactor,  rough,  1);
        bgfx_set_uniform(u_normalScale,      nscale, 1);
        bgfx_set_uniform(u_emissiveFactor,   emfac,  1);
        bgfx_set_uniform(u_emissiveStrength, emstr,  1);
        bgfx_set_uniform(u_hasBasecolor,     hbc,    1);
        bgfx_set_uniform(u_hasMR,            hmr,    1);
        bgfx_set_uniform(u_hasNormal,        hnm,    1);
        bgfx_set_uniform(u_hasEmissive,      hem,    1);

        bgfx_set_texture(7,  s_texBasecolor, m->material.tex_basecolor, UINT32_MAX);
        bgfx_set_texture(8,  s_texMR,        m->material.tex_mr,        UINT32_MAX);
        bgfx_set_texture(9,  s_texNormal,    m->material.tex_normal,    UINT32_MAX);
        bgfx_set_texture(10, s_texEmissive,  m->material.tex_emissive,  UINT32_MAX);
        bgfx_set_texture(11, s_texOcclusion, m->material.tex_occlusion, UINT32_MAX);

        bgfx_set_vertex_buffer(0, m->vb, 0, UINT32_MAX);
        bgfx_set_index_buffer(m->ib, 0, UINT32_MAX);

        uint64_t state = BGFX_STATE_WRITE_RGB
                       | BGFX_STATE_WRITE_A
                       | BGFX_STATE_WRITE_Z
                       | BGFX_STATE_DEPTH_TEST_LESS
                       | BGFX_STATE_MSAA;
        /* cull off for debug — was: if (!m->material.double_sided) state |= BGFX_STATE_CULL_CCW; */
        bgfx_set_state(state, 0);
        bgfx_submit(VIEW_SCENE, g_pbr_pass.prog, 0, false);
    }
}

static void draw_sky(void) {
    /* Build the sky view-proj (same camera, so reuse g_viewproj inverted). */
    mat4 inv_vp;
    {
        const float* m = g_viewproj.m;
        float inv[16];
        inv[0]  =  m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15] + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
        inv[4]  = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15] - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
        inv[8]  =  m[4]*m[9]*m[15]  - m[4]*m[11]*m[13] - m[8]*m[5]*m[15] + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
        inv[12] = -m[4]*m[9]*m[14]  + m[4]*m[10]*m[13] + m[8]*m[5]*m[14] - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
        inv[1]  = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15] - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
        inv[5]  =  m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15] + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
        inv[9]  = -m[0]*m[9]*m[15]  + m[0]*m[11]*m[13] + m[8]*m[1]*m[15] - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
        inv[13] =  m[0]*m[9]*m[14]  - m[0]*m[10]*m[13] - m[8]*m[1]*m[14] + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
        inv[2]  =  m[1]*m[6]*m[15]  - m[1]*m[7]*m[14]  - m[5]*m[2]*m[15] + m[5]*m[3]*m[14] + m[13]*m[2]*m[7]  - m[13]*m[3]*m[6];
        inv[6]  = -m[0]*m[6]*m[15]  + m[0]*m[7]*m[14]  + m[4]*m[2]*m[15] - m[4]*m[3]*m[14] - m[12]*m[2]*m[7]  + m[12]*m[3]*m[6];
        inv[10] =  m[0]*m[5]*m[15]  - m[0]*m[7]*m[13]  - m[4]*m[1]*m[15] + m[4]*m[3]*m[13] + m[12]*m[1]*m[7]  - m[12]*m[3]*m[5];
        inv[14] = -m[0]*m[5]*m[14]  + m[0]*m[6]*m[13]  + m[4]*m[1]*m[14] - m[4]*m[2]*m[13] - m[12]*m[1]*m[6]  + m[12]*m[2]*m[5];
        inv[3]  = -m[1]*m[6]*m[11]  + m[1]*m[7]*m[10]  + m[5]*m[2]*m[11] - m[5]*m[3]*m[10] - m[9]*m[2]*m[7]   + m[9]*m[3]*m[6];
        inv[7]  =  m[0]*m[6]*m[11]  - m[0]*m[7]*m[10]  - m[4]*m[2]*m[11] + m[4]*m[3]*m[10] + m[8]*m[2]*m[7]   - m[8]*m[3]*m[6];
        inv[11] = -m[0]*m[5]*m[11]  + m[0]*m[7]*m[9]   + m[4]*m[1]*m[11] - m[4]*m[3]*m[9]  - m[8]*m[1]*m[7]   + m[8]*m[3]*m[5];
        inv[15] =  m[0]*m[5]*m[10]  - m[0]*m[6]*m[9]   - m[4]*m[1]*m[10] + m[4]*m[2]*m[9]  + m[8]*m[1]*m[6]   - m[8]*m[2]*m[5];

        float det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];
        float idet = 1.0f / det;
        for (int i = 0; i < 16; ++i) inv_vp.m[i] = inv[i] * idet;
    }

    bgfx_set_uniform(sky_u_invViewProj, inv_vp.m, 1);

    float inv_res[4] = { 1.0f / (float)W, 1.0f / (float)H, 0, 0 };
    bgfx_set_uniform(sky_u_invResolution, inv_res, 1);

    float hdri_ok[4] = { g_hdr_env.valid ? 1.0f : 0.0f, 0, 0, 0 };
    bgfx_set_uniform(sky_u_hdriValid, hdri_ok, 1);

    if (g_hdr_env.valid) {
        bgfx_set_texture(0, sky_s_hdri, g_hdr_env.tex, UINT32_MAX);
    }

    bgfx_set_state(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_LEQUAL, 0);
    bgfx_set_vertex_count(3);
    bgfx_submit(VIEW_SKY, g_sky_pass.prog, 0, false);
}

static void draw_tonemap(void) {
    bgfx_set_texture(0, tm_s_scene, g_scene_fb.color, UINT32_MAX);
    float inv_size[4] = { 1.0f / (float)W, 1.0f / (float)H, 0, 0 };
    bgfx_set_uniform(tm_u_invSize, inv_size, 1);
    bgfx_set_state(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A, 0);
    bgfx_set_vertex_count(3);
    bgfx_submit(VIEW_TONEMAP, g_tonemap_pass.prog, 0, false);
}

/* ------------------------------------------------------------------ */
/* Frame loop                                                         */
/* ------------------------------------------------------------------ */

static void frame(void) {
    double now = glfwGetTime();
    float dt = (float)(now - g_last_time);
    if (dt <= 0.0f || dt > 0.25f) dt = 0.016f;
    g_last_time = now;
    g_time += dt;

    glfwPollEvents();

    bgfx_dbg_text_clear(0, false);
    bgfx_dbg_text_printf(0, 0,  0x0f, "scene fb: %dx%d", g_scene_fb.width, g_scene_fb.height);
    bgfx_dbg_text_printf(0, 1,  0x0f, "meshes:   %d", g_model.count);
    bgfx_dbg_text_printf(0, 2,  0x0f, "cam:      %.1f %.1f %.1f", g_cam_pos.x, g_cam_pos.y, g_cam_pos.z);
    bgfx_dbg_text_printf(0, 3,  0x0f, "jet:      %.1f %.1f %.1f", game_jet_position().x, game_jet_position().y, game_jet_position().z);

    /* Update the aircraft state and camera. */
    game_update(dt);
    game_prepare_transform();

    /* Point the camera at the jet. */
    g_cam_pos = game_camera_pos();
    vec3 cam_target = game_camera_target();
    g_view = m4_look_at(g_cam_pos, cam_target, v3(0.0f, 1.0f, 0.0f));
    g_proj = m4_perspective(50.0f * 3.14159265f / 180.0f,
                            (float)W / (float)H, 0.1f, 200.0f);
    g_viewproj = m4_mul(g_proj, g_view);

    /* View setup. Every active view must be touched this frame. */
    bgfx_set_view_frame_buffer(VIEW_SHADOW, g_shadow_fb.fbh);
    bgfx_set_view_rect(VIEW_SHADOW, 0, 0, 2048, 2048, 0.0f, 1.0f);
    bgfx_set_view_clear(VIEW_SHADOW,
        BGFX_CLEAR_DEPTH, 0, 1.0f, 0);

    fb_bind_view(&g_scene_fb, VIEW_SCENE);
    bgfx_set_view_clear(VIEW_SCENE,
        BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x101014ff, 1.0f, 0);

    fb_bind_view(&g_scene_fb, VIEW_SKY);
    bgfx_set_view_clear(VIEW_SKY, BGFX_CLEAR_NONE, 0, 1.0f, 0);

    fb_bind_backbuffer(VIEW_TONEMAP, W, H);
    bgfx_set_view_clear(VIEW_TONEMAP, BGFX_CLEAR_COLOR, 0x000000ff, 1.0f, 0);

    /* Passes in order */
    draw_model_shadow();
    bgfx_touch(VIEW_SHADOW);

    draw_model_pbr();
    bgfx_touch(VIEW_SCENE);

    draw_sky();
    bgfx_touch(VIEW_SKY);

    draw_tonemap();
    bgfx_touch(VIEW_TONEMAP);

    bgfx_frame(false);
}

/* ------------------------------------------------------------------ */
/* Main                                                               */
/* ------------------------------------------------------------------ */

int main(void) {
    init_window();
    init_bgfx();
    init_framebuffers();
    init_passes();
    init_uniforms();
    init_camera();

    /* Load the jet model (same .glb the WebGL build uses). */
    g_model = model_load_glb("assets/model.glb");
    if (g_model.count == 0) {
        fprintf(stderr, "failed to load assets/model.glb\n");
        return 1;
    }
    {
        vec3 c = model_center(&g_model);
        vec3 e = model_extent(&g_model);
        float longest = e.x;
        if (e.y > longest) longest = e.y;
        if (e.z > longest) longest = e.z;
        float sc = (longest > 1e-6f) ? (3.0f / longest) : 1.0f;
        mat4 S = m4_scale(v3(sc, sc, sc));
        mat4 T = m4_translate(v3(-c.x * sc, -c.y * sc, -c.z * sc));
        g_model_base = m4_mul(T, S);
        printf("[bgfx] model auto-scaled by %.4f (longest was %.2f)\n", sc, longest);
    }

    /* Load HDRI for the sky pass. */
    g_hdr_env = hdr_env_load("assets/sky.hdr");
    if (!g_hdr_env.valid) {
        fprintf(stderr, "WARN: sky.hdr failed to load; sky will use fallback\n");
    }

    /* game_init() sets up the jet's aircraft state. */
    game_init();

    printf("[bgfx] loaded model with %d meshes\n", g_model.count);
    printf("[bgfx] starting main loop\n");

    while (!glfwWindowShouldClose(g_window)) {
        frame();
    }

    /* Cleanup */
    pass_destroy(&g_pbr_pass);
    pass_destroy(&g_sky_pass);
    pass_destroy(&g_shadow_pass);
    pass_destroy(&g_tonemap_pass);
    fb_destroy(&g_scene_fb);
    fb_destroy(&g_shadow_fb);
    hdr_env_destroy(&g_hdr_env);
    model_destroy(&g_model);
    bgfx_shutdown();
    glfwDestroyWindow(g_window);
    glfwTerminate();
    return 0;
}
