#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <bgfx/c99/bgfx.h>

typedef enum {
    FB_RGBA8,
    FB_RGBA16F,
    FB_RGBA32F,
    FB_R8,
    FB_D24S8,
} FBFormat;

typedef struct {
    bgfx_frame_buffer_handle_t fbh;
    bgfx_texture_handle_t      color;    /* attachment 0 */
    bgfx_texture_handle_t      color1;   /* attachment 1 (G-buffer albedo) */
    bgfx_texture_handle_t      color2;   /* attachment 2 (G-buffer normal) */
    bgfx_texture_handle_t      color3;   /* attachment 3 (G-buffer emissive) */
    bgfx_texture_handle_t      depth;    /* depth texture (owned or borrowed) */
    int                        width;
    int                        height;
    FBFormat                   format;
    int                        attachment_count;
    int                        owns_depth;
    int                        valid;
} Framebuffer;

Framebuffer fb_create(int width, int height, FBFormat fmt);
Framebuffer fb_create_with_depth_tex(int width, int height, FBFormat fmt);
Framebuffer fb_create_gbuffer(int width, int height, bgfx_texture_handle_t depth_tex);
void        fb_destroy(Framebuffer* fb);

/* Bind an FB to a view. After this, bgfx_submit(view_id, ...) targets it. */
void fb_bind_view(Framebuffer* fb, bgfx_view_id_t view);

/* Bind the backbuffer to a view (screen). */
void fb_bind_backbuffer(bgfx_view_id_t view, int w, int h);

/* Convenience: get the texture handle at attachment index. */
bgfx_texture_handle_t fb_attachment(Framebuffer* fb, int index);

#endif
