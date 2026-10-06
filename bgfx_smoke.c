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

static void* read_file(const char* path, long* out_size) {
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "read_file: cannot open %s\n", path); return NULL; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    void* buf = malloc(sz);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, sz, f) != (size_t)sz) { free(buf); fclose(f); return NULL; }
    fclose(f);
    if (out_size) *out_size = sz;
    return buf;
}

static void glfw_error(int code, const char* desc) {
    fprintf(stderr, "GLFW error %d: %s\n", code, desc);
}

int main(void) {
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    glfwSetErrorCallback(glfw_error);
    if (!glfwInit()) { fprintf(stderr, "glfwInit failed\n"); return 1; }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* w = glfwCreateWindow(960, 720, "bgfx smoke", NULL, NULL);
    if (!w) { fprintf(stderr, "glfwCreateWindow failed\n"); glfwTerminate(); return 1; }

    bgfx_init_t init;
    bgfx_init_ctor(&init);
    init.type = BGFX_RENDERER_TYPE_OPENGL;

#if defined(__linux__)
    init.platformData.type    = BGFX_NATIVE_WINDOW_HANDLE_TYPE_DEFAULT;
    init.platformData.context = NULL;
    init.platformData.queue   = NULL;
    init.swapChain.nwh    = (void*)(uintptr_t)glfwGetX11Window(w);
    init.swapChain.ndt    = glfwGetX11Display();
    init.swapChain.width  = 960;
    init.swapChain.height = 720;
    init.swapChain.flags  = BGFX_SWAP_CHAIN_NONE;
    init.swapChain.formatColor        = BGFX_TEXTURE_FORMAT_BGRA8;
    init.swapChain.formatDepthStencil = BGFX_TEXTURE_FORMAT_D24S8;
#endif

    if (!bgfx_init(&init)) {
        fprintf(stderr, "bgfx_init failed\n");
        glfwDestroyWindow(w); glfwTerminate(); return 1;
    }

    /* Load compiled shader binaries. */
    long vs_sz = 0, fs_sz = 0;
    void* vs_bytes = read_file("shaders/bgfx_test/fullscreen.vert.bin", &vs_sz);
    void* fs_bytes = read_file("shaders/bgfx_test/solid.frag.bin", &fs_sz);
    if (!vs_bytes || !fs_bytes) {
        fprintf(stderr, "shader load failed\n");
        bgfx_shutdown(); glfwDestroyWindow(w); glfwTerminate(); return 1;
    }

    bgfx_shader_handle_t vs = bgfx_create_shader(bgfx_copy(vs_bytes, (uint32_t)vs_sz));
    bgfx_shader_handle_t fs = bgfx_create_shader(bgfx_copy(fs_bytes, (uint32_t)fs_sz));
    free(vs_bytes); free(fs_bytes);

    /* bgfx_is_shader_valid isn't in the C99 bindings; check handle != invalid. */
    if (vs.idx == UINT16_MAX || fs.idx == UINT16_MAX) {
        fprintf(stderr, "shader create failed (vs=%u fs=%u)\n", vs.idx, fs.idx);
        bgfx_shutdown(); glfwDestroyWindow(w); glfwTerminate(); return 1;
    }

    bgfx_program_handle_t prog = bgfx_create_program(vs, fs, true);
    if (prog.idx == UINT16_MAX) {
        fprintf(stderr, "program create failed\n");
        bgfx_shutdown(); glfwDestroyWindow(w); glfwTerminate(); return 1;
    }

    bgfx_set_view_clear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                        0x203040ff, 1.0f, 0);

    while (!glfwWindowShouldClose(w)) {
        glfwPollEvents();
        bgfx_set_view_rect(0, 0, 0, 960, 720, 0.0f, 1.0f);

        /* 3 vertices, no vertex buffer — gl_VertexID builds the triangle. */
        bgfx_set_vertex_count(3);
        bgfx_submit(0, prog, 0, false);

        bgfx_touch(0);
        bgfx_frame(false);
    }

    bgfx_destroy_program(prog);
    bgfx_destroy_shader(vs);
    bgfx_destroy_shader(fs);
    bgfx_shutdown();
    glfwDestroyWindow(w);
    glfwTerminate();
    return 0;
}
