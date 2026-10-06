#include <bgfx/c99/bgfx.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pass.h"
#include "framebuffer.h"

#if defined(__linux__)
#  define GLFW_EXPOSE_NATIVE_X11
#  define GLFW_EXPOSE_NATIVE_GLX
#  include <GLFW/glfw3native.h>
#endif

#define W 960
#define H 720

enum {
    VIEW_INTO_FB   = 0,   /* render gradient into g_scene_fb */
    VIEW_FROM_FB   = 1,   /* sample g_scene_fb, tonemap to screen */
};

static char* read_whole_file(const char* path, size_t* out_sz) {
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return NULL; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* b = malloc(sz + 1);
    if (fread(b, 1, sz, f) != (size_t)sz) { /* best-effort */ }
    b[sz] = 0;
    fclose(f);
    if (out_sz) *out_sz = (size_t)sz;
    return b;
}

/* asset_load is called by pass.c */


/* Gradient generator (same as before, HDR values). */
static void make_gradient(float* p, int w, int h) {
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float t = (float)x / (float)(w - 1);
            int i = (y * w + x) * 4;
            p[i+0] = (1.0f - t) * 2.0f;
            p[i+1] = 1.0f - (t - 0.5f) * (t - 0.5f) * 4.0f;
            p[i+2] = t * 2.0f;
            p[i+3] = 1.0f;
        }
    }
}

int main(void) {
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    if (!glfwInit()) { fprintf(stderr, "glfwInit failed\n"); return 1; }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* w = glfwCreateWindow(W, H, "bgfx pipeline test", NULL, NULL);
    if (!w) { fprintf(stderr, "create window failed\n"); glfwTerminate(); return 1; }

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

    /* ---- 1. An offscreen framebuffer to render into ---- */
    Framebuffer scene_fb = fb_create(W, H, FB_RGBA16F);
    if (!scene_fb.valid) { fprintf(stderr, "fb_create failed\n"); return 1; }
    printf("[test] scene_fb color=%u depth=%u\n",
           scene_fb.color.idx, scene_fb.depth.idx);

    /* ---- 2. A source texture holding the gradient ---- */
    float* pixels = malloc(W * H * 4 * sizeof(float));
    make_gradient(pixels, W, H);
    const bgfx_memory_t* tex_mem = bgfx_copy(pixels, (uint32_t)(W * H * 4 * sizeof(float)));
    free(pixels);

    bgfx_texture_handle_t gradient_tex = bgfx_create_texture_2d(
        (uint16_t)W, (uint16_t)H, false, 1,
        BGFX_TEXTURE_FORMAT_RGBA32F,
        BGFX_TEXTURE_NONE,
        tex_mem, 0);
    if (gradient_tex.idx == UINT16_MAX) { fprintf(stderr, "gradient tex fail\n"); return 1; }

    /* ---- 3. Passes ---- */
    Pass blit = pass_create_from_bin(
        "shaders/out/gl/fullscreen.vert.bin",
        "shaders/out/gl/blit_tonemap.frag.bin");
    if (!blit.valid) { fprintf(stderr, "blit pass failed\n"); return 1; }

    /* ---- 4. Uniforms ---- */
    bgfx_uniform_handle_t u_scene   = bgfx_create_uniform(
        "s_scene",   BGFX_UNIFORM_TYPE_SAMPLER, 1);
    bgfx_uniform_handle_t u_invSize = bgfx_create_uniform(
        "u_invSize", BGFX_UNIFORM_TYPE_VEC4, 1);

    float inv_size[4] = { 1.0f / (float)W, 1.0f / (float)H, 0.0f, 0.0f };

    /* ---- 5. Frame loop ---- */
    while (!glfwWindowShouldClose(w)) {
        glfwPollEvents();

        /* --- Pass A: render gradient INTO scene_fb ---
           We treat the gradient texture as if it were the scene: sample it
           with a pass-through. Since blit_tonemap does ACES+sRGB, the FB
           will hold *tonemapped* values. That's fine for the test — we
           just want to prove the FB can be rendered into and then read. */
        fb_bind_view(&scene_fb, VIEW_INTO_FB);
        bgfx_set_view_clear(VIEW_INTO_FB,
            BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x000000ff, 1.0f, 0);
        bgfx_set_texture(0, u_scene, gradient_tex, UINT32_MAX);
        bgfx_set_uniform(u_invSize, inv_size, 1);
        pass_draw(&blit, W, H);
        bgfx_submit(VIEW_INTO_FB, blit.prog, 0, false);

        /* --- Pass B: sample scene_fb's color attachment, tonemap to screen ---
           Because blit_tonemap already tonemapped once, this will look
           washed-out. That's expected — we're just testing the plumbing. */
        fb_bind_backbuffer(VIEW_FROM_FB, W, H);
        bgfx_set_view_clear(VIEW_FROM_FB,
            BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x101010ff, 1.0f, 0);
        bgfx_set_texture(0, u_scene, scene_fb.color, UINT32_MAX);
        bgfx_set_uniform(u_invSize, inv_size, 1);
        pass_draw(&blit, W, H);
        bgfx_submit(VIEW_FROM_FB, blit.prog, 0, false);

        bgfx_frame(false);
    }

    pass_destroy(&blit);
    bgfx_destroy_uniform(u_scene);
    bgfx_destroy_uniform(u_invSize);
    bgfx_destroy_texture(gradient_tex);
    fb_destroy(&scene_fb);
    bgfx_shutdown();
    glfwDestroyWindow(w);
    glfwTerminate();
    return 0;
}
