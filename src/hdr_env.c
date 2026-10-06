#include "hdr_env.h"
#include "asset.h"
#include "stb_image.h"
#include <stdio.h>
#include <stdlib.h>

HDREnv hdr_env_load(const char* path) {
    HDREnv env = {0};

    size_t len = 0;
    char* bytes = asset_load(path, &len);
    if (!bytes) {
        fprintf(stderr, "hdr_env: cannot load %s\n", path);
        return env;
    }

    int w = 0, h = 0, comp = 0;
    float* data = stbi_loadf_from_memory((const unsigned char*)bytes, (int)len,
                                          &w, &h, &comp, 4);
    free(bytes);
    if (!data) {
        fprintf(stderr, "hdr_env: stbi_loadf failed for %s\n", path);
        return env;
    }

    printf("hdr_env: loaded %s (%dx%d, %d channels)\n", path, w, h, comp);

    uint64_t flags = BGFX_TEXTURE_NONE
                   | BGFX_SAMPLER_U_CLAMP
                   | BGFX_SAMPLER_V_CLAMP
                   | BGFX_SAMPLER_MIN_ANISOTROPIC;

    const bgfx_memory_t* mem = bgfx_copy(data, (uint32_t)(w * h * 4 * sizeof(float)));

    /* RGBA32F for HDR sky. bgfx handles the float upload. */
    env.tex = bgfx_create_texture_2d(
        (uint16_t)w, (uint16_t)h, false, 1,
        BGFX_TEXTURE_FORMAT_RGBA32F,
        flags,
        mem, 0);

    stbi_image_free(data);

    if (env.tex.idx == UINT16_MAX) {
        fprintf(stderr, "hdr_env: texture creation failed\n");
        return env;
    }

    env.width = w;
    env.height = h;
    env.valid = 1;
    return env;
}

void hdr_env_destroy(HDREnv* h) {
    if (h->tex.idx && h->tex.idx != UINT16_MAX) bgfx_destroy_texture(h->tex);
    h->tex.idx = UINT16_MAX;
    h->valid = 0;
}
