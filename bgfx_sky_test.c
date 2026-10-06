#include <bgfx/c99/bgfx.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pass.h"
#include "hdr_env.h"
#include "math3d.h"

#if defined(__linux__)
#  define GLFW_EXPOSE_NATIVE_X11
#  define GLFW_EXPOSE_NATIVE_GLX
#  include <GLFW/glfw3native.h>
#endif

#define W 960
#define H 720

/* asset_load is called by pass.c and hdr_env.c */
static char* read_whole_file(const char* path, size_t* out_sz) {
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return NULL; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* b = malloc(sz + 1);
    if (fread(b, 1, sz, f) != (size_t)sz) { /* ignore */ }
    b[sz] = 0;
    fclose(f);
    if (out_sz) *out_sz = (size_t)sz;
    return b;
}
char* asset_load(const char* path, size_t* out_size) {
    return read_whole_file(path, out_size);
}

/* A fixed camera looking slightly up and forward. */
static mat4 make_view(void) {
    vec3 eye    = v3(0.0f, 0.0f, 0.0f);
    vec3 target = v3(0.0f, 0.2f, -1.0f);
    vec3 up     = v3(0.0f, 1.0f, 0.0f);
    return m4_look_at(eye, target, up);
}

static mat4 make_proj(float aspect) {
    return m4_perspective(60.0f * 3.14159265f / 180.0f, aspect, 0.1f, 200.0f);
}

/* C-compatible 4x4 inverse. Standard cofactor expansion. */
static mat4 mat4_inverse(mat4 m) {
    const float* a = m.m;
    float inv[16];
    inv[0]  =  a[5]*a[10]*a[15] - a[5]*a[11]*a[14] - a[9]*a[6]*a[15] + a[9]*a[7]*a[14] + a[13]*a[6]*a[11] - a[13]*a[7]*a[10];
    inv[4]  = -a[4]*a[10]*a[15] + a[4]*a[11]*a[14] + a[8]*a[6]*a[15] - a[8]*a[7]*a[14] - a[12]*a[6]*a[11] + a[12]*a[7]*a[10];
    inv[8]  =  a[4]*a[9]*a[15]  - a[4]*a[11]*a[13] - a[8]*a[5]*a[15] + a[8]*a[7]*a[13] + a[12]*a[5]*a[11] - a[12]*a[7]*a[9];
    inv[12] = -a[4]*a[9]*a[14]  + a[4]*a[10]*a[13] + a[8]*a[5]*a[14] - a[8]*a[6]*a[13] - a[12]*a[5]*a[10] + a[12]*a[6]*a[9];
    inv[1]  = -a[1]*a[10]*a[15] + a[1]*a[11]*a[14] + a[9]*a[2]*a[15] - a[9]*a[3]*a[14] - a[13]*a[2]*a[11] + a[13]*a[3]*a[10];
    inv[5]  =  a[0]*a[10]*a[15] - a[0]*a[11]*a[14] - a[8]*a[2]*a[15] + a[8]*a[3]*a[14] + a[12]*a[2]*a[11] - a[12]*a[3]*a[10];
    inv[9]  = -a[0]*a[9]*a[15]  + a[0]*a[11]*a[13] + a[8]*a[1]*a[15] - a[8]*a[3]*a[13] - a[12]*a[1]*a[11] + a[12]*a[3]*a[9];
    inv[13] =  a[0]*a[9]*a[14]  - a[0]*a[10]*a[13] - a[8]*a[1]*a[14] + a[8]*a[2]*a[13] + a[12]*a[1]*a[10] - a[12]*a[2]*a[9];
    inv[2]  =  a[1]*a[6]*a[15]  - a[1]*a[7]*a[14]  - a[5]*a[2]*a[15] + a[5]*a[3]*a[14] + a[13]*a[2]*a[7]  - a[13]*a[3]*a[6];
    inv[6]  = -a[0]*a[6]*a[15]  + a[0]*a[7]*a[14]  + a[4]*a[2]*a[15] - a[4]*a[3]*a[14] - a[12]*a[2]*a[7]  + a[12]*a[3]*a[6];
    inv[10] =  a[0]*a[5]*a[15]  - a[0]*a[7]*a[13]  - a[4]*a[1]*a[15] + a[4]*a[3]*a[13] + a[12]*a[1]*a[7]  - a[12]*a[3]*a[5];
    inv[14] = -a[0]*a[5]*a[14]  + a[0]*a[6]*a[13]  + a[4]*a[1]*a[14] - a[4]*a[2]*a[13] - a[12]*a[1]*a[6]  + a[12]*a[2]*a[5];
    inv[3]  = -a[1]*a[6]*a[11]  + a[1]*a[7]*a[10]  + a[5]*a[2]*a[11] - a[5]*a[3]*a[10] - a[9]*a[2]*a[7]   + a[9]*a[3]*a[6];
    inv[7]  =  a[0]*a[6]*a[11]  - a[0]*a[7]*a[10]  - a[4]*a[2]*a[11] + a[4]*a[3]*a[10] + a[8]*a[2]*a[7]   - a[8]*a[3]*a[6];
    inv[11] = -a[0]*a[5]*a[11]  + a[0]*a[7]*a[9]   + a[4]*a[1]*a[11] - a[4]*a[3]*a[9]  - a[8]*a[1]*a[7]   + a[8]*a[3]*a[5];
    inv[15] =  a[0]*a[5]*a[10]  - a[0]*a[6]*a[9]   - a[4]*a[1]*a[10] + a[4]*a[2]*a[9]  + a[8]*a[1]*a[6]   - a[8]*a[2]*a[5];

    float det = a[0]*inv[0] + a[1]*inv[4] + a[2]*inv[8] + a[3]*inv[12];
    float idet = 1.0f / det;
    mat4 r;
    for (int i = 0; i < 16; ++i) r.m[i] = inv[i] * idet;
    return r;
}

int main(void) {
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    if (!glfwInit()) { fprintf(stderr, "glfwInit failed\n"); return 1; }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* w = glfwCreateWindow(W, H, "bgfx sky test", NULL, NULL);
    if (!w) { fprintf(stderr, "window failed\n"); glfwTerminate(); return 1; }

    bgfx_init_t init;
    bgfx_init_ctor(&init);
    init.type = BGFX_RENDERER_TYPE_OPENGL;
    init.platformData.type = BGFX_NATIVE_WINDOW_HANDLE_TYPE_DEFAULT;
    init.swapChain.nwh    = (void*)(uintptr_t)glfwGetX11Window(w);
    init.swapChain.ndt    = glfwGetX11Display();
    init.swapChain.width  = W;
    init.swapChain.height = H;
    init.swapChain.flags  = BGFX_SWAP_CHAIN_NONE;
    init.swapChain.formatColor        = BGFX_TEXTURE_FORMAT_BGRA8;
    init.swapChain.formatDepthStencil = BGFX_TEXTURE_FORMAT_D24S8;

    if (!bgfx_init(&init)) { fprintf(stderr, "bgfx_init failed\n"); glfwTerminate(); return 1; }

    /* Load HDRI. Adjust the path to wherever your sky.hdr lives. */
    HDREnv env = hdr_env_load("assets/sky.hdr");
    if (!env.valid) {
        fprintf(stderr, "WARN: sky.hdr failed to load — will use gradient fallback\n");
    }

    Pass sky = pass_create_from_bin(
        "shaders/out/gl/fullscreen.vert.bin",
        "shaders/out/gl/sky_background.frag.bin");
    if (!sky.valid) { fprintf(stderr, "sky pass failed\n"); return 1; }

    bgfx_uniform_handle_t u_invVP   = bgfx_create_uniform("u_customInvViewProj",
                                                          BGFX_UNIFORM_TYPE_MAT4, 1);
    bgfx_uniform_handle_t u_invRes  = bgfx_create_uniform("u_invResolution",
                                                          BGFX_UNIFORM_TYPE_VEC4, 1);
    bgfx_uniform_handle_t u_hdriOK  = bgfx_create_uniform("u_hdriValid",
                                                          BGFX_UNIFORM_TYPE_VEC4, 1);
    bgfx_uniform_handle_t s_hdri    = bgfx_create_uniform("s_hdri",
                                                          BGFX_UNIFORM_TYPE_SAMPLER, 1);

    /* Camera matrices. Fixed for the test. */
    mat4 view = make_view();
    mat4 proj = make_proj((float)W / (float)H);
    mat4 vp   = m4_mul(proj, view);
    mat4 inv  = mat4_inverse(vp);

    float inv_res[4] = { 1.0f / (float)W, 1.0f / (float)H, 0.0f, 0.0f };
    float hdri_ok[4] = { env.valid ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f };

    bgfx_set_view_clear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x000000ff, 1.0f, 0);
    bgfx_set_view_rect(0, 0, 0, W, H, 0.0f, 1.0f);

    while (!glfwWindowShouldClose(w)) {
        glfwPollEvents();

        bgfx_set_uniform(u_invVP,  inv.m,     1);
        bgfx_set_uniform(u_invRes, inv_res,   1);
        bgfx_set_uniform(u_hdriOK, hdri_ok,   1);
        if (env.valid) bgfx_set_texture(0, s_hdri, env.tex, UINT32_MAX);

        pass_use(&sky);
        pass_draw(&sky, W, H);
        bgfx_submit(0, sky.prog, 0, false);
        bgfx_touch(0);
        bgfx_frame(false);
    }

    pass_destroy(&sky);
    hdr_env_destroy(&env);
    bgfx_destroy_uniform(u_invVP);
    bgfx_destroy_uniform(u_invRes);
    bgfx_destroy_uniform(u_hdriOK);
    bgfx_destroy_uniform(s_hdri);
    bgfx_shutdown();
    glfwDestroyWindow(w);
    glfwTerminate();
    return 0;
}
