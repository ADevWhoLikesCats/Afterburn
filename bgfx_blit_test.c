#include <bgfx/c99/bgfx.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#if defined(__linux__)
#  define GLFW_EXPOSE_NATIVE_X11
#  define GLFW_EXPOSE_NATIVE_GLX
#  include <GLFW/glfw3native.h>
#endif

#define W 960
#define H 720

static void* read_file(const char* path, long* out_size) {
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return NULL; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    void* buf = malloc(sz);
    if (fread(buf, 1, sz, f) != (size_t)sz) { free(buf); fclose(f); return NULL; }
    fclose(f);
    if (out_size) *out_size = sz;
    return buf;
}

/* Build a horizontal gradient texture in HDR range.
   Left = bright red (2.0), middle = green, right = bright blue. */
static void make_gradient(float* pixels, int w, int h) {
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float t = (float)x / (float)(w - 1);
            float r = (1.0f - t) * 2.0f;
            float g = 1.0f - (t - 0.5f) * (t - 0.5f) * 4.0f;
            float b = t * 2.0f;
            int i = (y * w + x) * 4;
            pixels[i+0] = r;
            pixels[i+1] = g;
            pixels[i+2] = b;
            pixels[i+3] = 1.0f;
        }
    }
}

int main(void) {
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    if (!glfwInit()) { fprintf(stderr, "glfwInit failed\n"); return 1; }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* w = glfwCreateWindow(W, H, "bgfx blit test", NULL, NULL);
    if (!w) { fprintf(stderr, "glfwCreateWindow failed\n"); glfwTerminate(); return 1; }

    bgfx_init_t init;
    bgfx_init_ctor(&init);
    init.type = BGFX_RENDERER_TYPE_OPENGL;
    init.platformData.type    = BGFX_NATIVE_WINDOW_HANDLE_TYPE_DEFAULT;
    init.swapChain.nwh    = (void*)(uintptr_t)glfwGetX11Window(w);
    init.swapChain.ndt    = glfwGetX11Display();
    init.swapChain.width  = W;
    init.swapChain.height = H;
    init.swapChain.flags  = BGFX_SWAP_CHAIN_NONE;
    init.swapChain.formatColor        = BGFX_TEXTURE_FORMAT_BGRA8;
    init.swapChain.formatDepthStencil = BGFX_TEXTURE_FORMAT_D24S8;

    if (!bgfx_init(&init)) { fprintf(stderr, "bgfx_init failed\n"); glfwTerminate(); return 1; }

    /* 1. Create HDR texture from CPU data */
    float* pixels = malloc(W * H * 4 * sizeof(float));
    make_gradient(pixels, W, H);

    const bgfx_memory_t* tex_mem = bgfx_copy(pixels, (uint32_t)(W * H * 4 * sizeof(float)));
    free(pixels);

    bgfx_texture_handle_t scene_tex = bgfx_create_texture_2d(
        (uint16_t)W, (uint16_t)H, false, 1,
        BGFX_TEXTURE_FORMAT_RGBA32F,
        BGFX_TEXTURE_NONE,
        tex_mem, 0);

    if (scene_tex.idx == UINT16_MAX) { fprintf(stderr, "tex create failed\n"); return 1; }

    /* 2. Create uniforms */
    bgfx_uniform_handle_t u_scene   = bgfx_create_uniform("s_scene",   BGFX_UNIFORM_TYPE_SAMPLER, 1);
    bgfx_uniform_handle_t u_invSize = bgfx_create_uniform("u_invSize", BGFX_UNIFORM_TYPE_VEC4,    1);

    /* 3. Load shaders and create program */
    long vs_sz = 0, fs_sz = 0;
    void* vs_bytes = read_file("shaders/out/gl/fullscreen.vert.bin", &vs_sz);
    void* fs_bytes = read_file("shaders/out/gl/blit_tonemap.frag.bin", &fs_sz);
    if (!vs_bytes || !fs_bytes) { fprintf(stderr, "shader load failed\n"); return 1; }

    bgfx_shader_handle_t vs = bgfx_create_shader(bgfx_copy(vs_bytes, (uint32_t)vs_sz));
    bgfx_shader_handle_t fs = bgfx_create_shader(bgfx_copy(fs_bytes, (uint32_t)fs_sz));
    free(vs_bytes); free(fs_bytes);

    bgfx_program_handle_t prog = bgfx_create_program(vs, fs, true);
    if (prog.idx == UINT16_MAX) { fprintf(stderr, "program failed\n"); return 1; }

    bgfx_set_view_clear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x000000ff, 1.0f, 0);

    /* 4. Frame loop */
    float inv_size[4] = { 1.0f / (float)W, 1.0f / (float)H, 0.0f, 0.0f };

    while (!glfwWindowShouldClose(w)) {
        glfwPollEvents();

        bgfx_set_view_rect(0, 0, 0, W, H, 0.0f, 1.0f);

        bgfx_set_texture(0, u_scene, scene_tex, UINT32_MAX);
        bgfx_set_uniform(u_invSize, inv_size, 1);

        bgfx_set_vertex_count(3);
        bgfx_submit(0, prog, 0, false);
        bgfx_touch(0);
        bgfx_frame(false);
    }

    bgfx_destroy_uniform(u_scene);
    bgfx_destroy_uniform(u_invSize);
    bgfx_destroy_texture(scene_tex);
    bgfx_destroy_program(prog);
    bgfx_destroy_shader(vs);
    bgfx_destroy_shader(fs);
    bgfx_shutdown();
    glfwDestroyWindow(w);
    glfwTerminate();
    return 0;
}
