#include "ground.h"
#include "texture.h"
#include "asset.h"
#include <stdlib.h>
#include <stdio.h>

/* How many times the asphalt texture repeats across the full plane.
   At half_size = 10000 (20000 units wide) and repeat = 400,
   each tile covers 50 units. Adjust to taste. */
#define GROUND_TEX_REPEAT 400.0f

Mesh ground_create(float y_level, float half_size) {
    Mesh m = {0};

    float y = y_level;
    float s = half_size;
    float r = GROUND_TEX_REPEAT;

    /* Interleaved: pos3, nrm3, uv2, tan4 = 12 floats */
    float verts[] = {
        /* x     y   z      nx  ny  nz     u     v      tx  ty  tz  tw */
        -s, y, -s,   0, 1, 0,   0, 0,    1, 0, 0, 1,
         s, y, -s,   0, 1, 0,   r, 0,    1, 0, 0, 1,
         s, y,  s,   0, 1, 0,   r, r,    1, 0, 0, 1,
        -s, y,  s,   0, 1, 0,   0, r,    1, 0, 0, 1,
    };
    unsigned short indices[] = { 0, 1, 2, 0, 2, 3 };

    glGenVertexArrays(1, &m.vao);
    glBindVertexArray(m.vao);

    glGenBuffers(1, &m.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12*sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 12*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 12*sizeof(float), (void*)(6*sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 12*sizeof(float), (void*)(8*sizeof(float)));

    glGenBuffers(1, &m.ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glBindVertexArray(0);

    m.index_count = 6 | 0x80000000;   /* 16-bit indices */
    m.model = m4_identity();

    /* Material: asphalt */
    m.material = (Material){0};
    m.material.basecolor_factor = v3(1.0f, 1.0f, 1.0f);
    m.material.metallic_factor  = 0.0f;
    m.material.roughness_factor = 0.85f;
    m.material.normal_scale     = 1.0f;
    m.material.occlusion_strength = 1.0f;
    m.material.emissive_strength  = 1.0f;

    /* Load the asphalt texture from the preloaded asset bundle. */
    size_t tex_len = 0;
    char* tex_bytes = asset_load("/assets/asphalt.jpg", &tex_len);
    if (tex_bytes) {
        m.material.tex_basecolor = texture_from_memory(
            (const unsigned char*)tex_bytes, (int)tex_len, 1);
        m.material.has_basecolor = 1;
        free(tex_bytes);
        printf("[ground] asphalt texture loaded (%zu bytes)\n", tex_len);
    } else {
        fprintf(stderr, "[ground] WARNING: could not load /assets/asphalt.jpg\n");
        m.material.tex_basecolor = texture_solid(128, 128, 128, 255, 1);
        m.material.has_basecolor = 1;
    }

    m.material.tex_normal    = texture_solid(128, 128, 255, 255, 0);
    m.material.tex_mr        = texture_solid(255, 140, 0, 255, 0);   /* G=rough 0.55, B=metal 0 */
    m.material.tex_emissive  = texture_solid(0, 0, 0, 255, 1);
    m.material.tex_occlusion = texture_solid(255, 255, 255, 255, 0);
    m.material.has_mr        = 0;   /* use the roughness factor */
    m.material.has_normal    = 0;
    return m;
}
