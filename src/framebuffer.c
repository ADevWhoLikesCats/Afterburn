#include "framebuffer.h"
#include <stdio.h>
#include <string.h>

static bgfx_texture_format_t to_bgfx_format(FBFormat fmt) {
    switch (fmt) {
        case FB_RGBA8:   return BGFX_TEXTURE_FORMAT_RGBA8;
        case FB_RGBA16F: return BGFX_TEXTURE_FORMAT_RGBA16F;
        case FB_RGBA32F: return BGFX_TEXTURE_FORMAT_RGBA32F;
        case FB_R8:      return BGFX_TEXTURE_FORMAT_R8;
        case FB_D24S8:   return BGFX_TEXTURE_FORMAT_D24S8;
    }
    return BGFX_TEXTURE_FORMAT_RGBA8;
}

static bgfx_texture_handle_t make_texture(int w, int h, FBFormat fmt, uint64_t flags) {
    /* For render targets, bgfx wants BGFX_TEXTURE_RT in flags. */
    return bgfx_create_texture_2d(
        (uint16_t)w, (uint16_t)h, false, 1,
        to_bgfx_format(fmt),
        BGFX_TEXTURE_RT | flags,
        NULL, 0);
}

Framebuffer fb_create(int w, int h, FBFormat fmt) {
    Framebuffer fb;
    memset(&fb, 0, sizeof(fb));
    fb.width = w; fb.height = h; fb.format = fmt;
    fb.attachment_count = 1;
    fb.owns_depth = 1;
    fb.valid = 0;

    fb.color = make_texture(w, h, fmt, BGFX_TEXTURE_NONE);
    fb.depth = make_texture(w, h, FB_D24S8, BGFX_TEXTURE_NONE);

    if (fb.color.idx == UINT16_MAX || fb.depth.idx == UINT16_MAX) {
        fprintf(stderr, "[fb] texture creation failed %dx%d\n", w, h);
        return fb;
    }

    bgfx_texture_handle_t attachments[2] = { fb.color, fb.depth };
    fb.fbh = bgfx_create_frame_buffer_from_handles(2, attachments, false);
    if (fb.fbh.idx == UINT16_MAX) {
        fprintf(stderr, "[fb] fb_create failed %dx%d\n", w, h);
        return fb;
    }

    fb.valid = 1;
    return fb;
}

Framebuffer fb_create_with_depth_tex(int w, int h, FBFormat fmt) {
    /* Same as fb_create, but marks depth as a separate texture we want
       to sample from later. In bgfx this is the same creation path —
       the difference is just that we keep the depth handle around. */
    return fb_create(w, h, fmt);
}

Framebuffer fb_create_gbuffer(int w, int h, bgfx_texture_handle_t borrowed_depth) {
    Framebuffer fb;
    memset(&fb, 0, sizeof(fb));
    fb.width = w; fb.height = h; fb.format = FB_RGBA16F;
    fb.attachment_count = 4;
    fb.owns_depth = 0;
    fb.valid = 0;

    fb.color  = make_texture(w, h, FB_RGBA16F, BGFX_TEXTURE_NONE);
    fb.color1 = make_texture(w, h, FB_RGBA16F, BGFX_TEXTURE_NONE);
    fb.color2 = make_texture(w, h, FB_RGBA16F, BGFX_TEXTURE_NONE);
    fb.color3 = make_texture(w, h, FB_RGBA16F, BGFX_TEXTURE_NONE);

    if (fb.color.idx  == UINT16_MAX || fb.color1.idx == UINT16_MAX ||
        fb.color2.idx == UINT16_MAX || fb.color3.idx == UINT16_MAX) {
        fprintf(stderr, "[gbuffer] texture creation failed\n");
        return fb;
    }

    fb.depth = borrowed_depth;

    bgfx_texture_handle_t attachments[5] = {
        fb.color, fb.color1, fb.color2, fb.color3, fb.depth
    };
    fb.fbh = bgfx_create_frame_buffer_from_handles(5, attachments, false);
    if (fb.fbh.idx == UINT16_MAX) {
        fprintf(stderr, "[gbuffer] create failed\n");
        return fb;
    }

    fb.valid = 1;
    printf("[gbuffer] created %dx%d att0=%u att1=%u att2=%u att3=%u\n",
           w, h, fb.color.idx, fb.color1.idx, fb.color2.idx, fb.color3.idx);
    return fb;
}

void fb_destroy(Framebuffer* fb) {
    if (!fb) return;
    if (fb->fbh.idx && fb->fbh.idx != UINT16_MAX) bgfx_destroy_frame_buffer(fb->fbh);
    if (fb->color.idx  && fb->color.idx  != UINT16_MAX) bgfx_destroy_texture(fb->color);
    if (fb->color1.idx && fb->color1.idx != UINT16_MAX) bgfx_destroy_texture(fb->color1);
    if (fb->color2.idx && fb->color2.idx != UINT16_MAX) bgfx_destroy_texture(fb->color2);
    if (fb->color3.idx && fb->color3.idx != UINT16_MAX) bgfx_destroy_texture(fb->color3);
    if (fb->owns_depth && fb->depth.idx && fb->depth.idx != UINT16_MAX)
        bgfx_destroy_texture(fb->depth);
    memset(fb, 0, sizeof(*fb));
}

void fb_bind_view(Framebuffer* fb, bgfx_view_id_t view) {
    bgfx_set_view_frame_buffer(view, fb->fbh);
    bgfx_set_view_rect(view, 0, 0, (uint16_t)fb->width, (uint16_t)fb->height, 0.0f, 1.0f);
}

void fb_bind_backbuffer(bgfx_view_id_t view, int w, int h) {
    bgfx_frame_buffer_handle_t no_fb = { UINT16_MAX };
    bgfx_set_view_frame_buffer(view, no_fb);
    bgfx_set_view_rect(view, 0, 0, (uint16_t)w, (uint16_t)h, 0.0f, 1.0f);
}

bgfx_texture_handle_t fb_attachment(Framebuffer* fb, int index) {
    switch (index) {
        case 0: return fb->color;
        case 1: return fb->color1;
        case 2: return fb->color2;
        case 3: return fb->color3;
        case 4: return fb->depth;
        default: { bgfx_texture_handle_t inv = { UINT16_MAX }; return inv; }
    }
}
