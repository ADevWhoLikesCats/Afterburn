#ifndef TEXTURE_H
#define TEXTURE_H

#include <bgfx/c99/bgfx.h>

/* Create a texture from an in-memory image (PNG/JPG).
   srgb=1 for basecolor/emissive, 0 for linear data (normal, MR, occlusion).
   Returns an invalid handle on failure. */
bgfx_texture_handle_t texture_from_memory(const unsigned char* bytes, int len, int srgb);

/* Create a 1x1 solid-color texture, useful as a fallback. */
bgfx_texture_handle_t texture_solid(unsigned char r, unsigned char g, unsigned char b, unsigned char a, int srgb);

#endif
