#include "texture.h"
#include "stb_image.h"
#include <stdio.h>
#include <stdlib.h>

bgfx_texture_handle_t texture_from_memory(const unsigned char* bytes, int len, int srgb) {
    int w = 0, h = 0, comp = 0;
    unsigned char* data = stbi_load_from_memory(bytes, len, &w, &h, &comp, 4);
    if (!data) {
        fprintf(stderr, "texture: stbi failed to decode %d bytes\n", len);
        bgfx_texture_handle_t inv = { UINT16_MAX };
        return inv;
    }

    uint64_t flags = BGFX_TEXTURE_NONE | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
    if (srgb) flags |= BGFX_TEXTURE_SRGB;

    const bgfx_memory_t* mem = bgfx_copy(data, (uint32_t)(w * h * 4));

    bgfx_texture_handle_t tex = bgfx_create_texture_2d(
        (uint16_t)w, (uint16_t)h, false, 1,
        BGFX_TEXTURE_FORMAT_RGBA8,
        flags,
        mem, 0);

    stbi_image_free(data);
    if (tex.idx == UINT16_MAX) {
        fprintf(stderr, "texture: bgfx_create_texture_2d failed for %dx%d\n", w, h);
    }
    return tex;
}

bgfx_texture_handle_t texture_solid(unsigned char r, unsigned char g, unsigned char b, unsigned char a, int srgb) {
    unsigned char px[4] = { r, g, b, a };
    uint64_t flags = BGFX_TEXTURE_NONE | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
    if (srgb) flags |= BGFX_TEXTURE_SRGB;

    const bgfx_memory_t* mem = bgfx_copy(px, 4);
    return bgfx_create_texture_2d(
        1, 1, false, 1,
        BGFX_TEXTURE_FORMAT_RGBA8,
        flags,
        mem, 0);
}
