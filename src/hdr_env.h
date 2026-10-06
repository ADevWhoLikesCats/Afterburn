#ifndef HDR_ENV_H
#define HDR_ENV_H

#include <bgfx/c99/bgfx.h>
#include "math3d.h"

typedef struct {
    bgfx_texture_handle_t tex;
    int    width;
    int    height;
    int    valid;
} HDREnv;

HDREnv hdr_env_load(const char* path);
void   hdr_env_destroy(HDREnv* h);

#endif
