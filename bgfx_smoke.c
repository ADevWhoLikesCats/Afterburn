#include <bgfx/c99/bgfx.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdint.h>

#if defined(__linux__)
#  define GLFW_EXPOSE_NATIVE_X11
#  define GLFW_EXPOSE_NATIVE_GLX
#  include <GLFW/glfw3native.h>
#endif

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

    /* Native window / display live on the swap chain in this bgfx version. */
#if defined(__linux__)
    init.platformData.type = BGFX_NATIVE_WINDOW_HANDLE_TYPE_DEFAULT;
    init.platformData.context = NULL;   /* let bgfx create the GL context */
    init.platformData.queue   = NULL;

    init.swapChain.nwh    = (void*)(uintptr_t)glfwGetX11Window(w);
    init.swapChain.ndt    = glfwGetX11Display();
    init.swapChain.width  = 960;
    init.swapChain.height = 720;
    init.swapChain.flags  = BGFX_SWAP_CHAIN_NONE;
    init.swapChain.formatColor = BGFX_TEXTURE_FORMAT_BGRA8;
    init.swapChain.formatDepthStencil = BGFX_TEXTURE_FORMAT_D24S8;
#endif

    if (!bgfx_init(&init)) {
        fprintf(stderr, "bgfx_init failed\n");
        glfwDestroyWindow(w);
        glfwTerminate();
        return 1;
    }

    bgfx_set_view_clear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                        0x203040ff, 1.0f, 0);

    while (!glfwWindowShouldClose(w)) {
        glfwPollEvents();
        bgfx_set_view_rect(0, 0, 0, 960, 720, 0.0f, 1.0f);
        bgfx_touch(0);
        bgfx_frame(false);
    }

    bgfx_shutdown();
    glfwDestroyWindow(w);
    glfwTerminate();
    return 0;
}
