#include "trail.h"
#include "asset.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Shader loading                                                      */
/* ------------------------------------------------------------------ */

static GLuint load_shader(GLenum type, const char* path) {
    size_t len = 0;
    char* src = asset_load(path, &len);
    if (!src) { fprintf(stderr, "trail: cannot load %s\n", path); abort(); }
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        fprintf(stderr, "trail: compile failed [%s]:\n%s\n", path, log);
        abort();
    }
    free(src);
    return s;
}

static GLuint link_prog(GLuint vs, GLuint fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(p, sizeof(log), NULL, log);
        fprintf(stderr, "trail: link failed:\n%s\n", log);
        abort();
    }
    return p;
}

/* ------------------------------------------------------------------ */
/* Creation                                                            */
/* ------------------------------------------------------------------ */

void trail_create(Trail* t) {
    memset(t, 0, sizeof(*t));
    t->head = 0;
    t->count = 0;

    GLuint vs = load_shader(GL_VERTEX_SHADER,   "/shaders/trail.vert");
    GLuint fs = load_shader(GL_FRAGMENT_SHADER, "/shaders/trail.frag");
    t->prog = link_prog(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    t->u_viewproj = glGetUniformLocation(t->prog, "uViewProj");
    t->u_campos   = glGetUniformLocation(t->prog, "uCamPos");
    t->u_tex      = glGetUniformLocation(t->prog, "uTex");
    t->u_opacity  = glGetUniformLocation(t->prog, "uOpacity");
    t->u_max_age  = glGetUniformLocation(t->prog, "uMaxAge");

    glGenVertexArrays(1, &t->vao);
    glGenBuffers(1, &t->vbo);
    glBindVertexArray(t->vao);
    glBindBuffer(GL_ARRAY_BUFFER, t->vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(float) * 16 * (TRAIL_MAX_POINTS - 1) * 4,
                 NULL, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(3*sizeof(float)));

    glBindVertexArray(0);
    printf("[trail] create: prog=%u vao=%u vbo=%u\n", t->prog, t->vao, t->vbo);
}


void trail_destroy(Trail* t) {
    if (t->vbo) glDeleteBuffers(1, &t->vbo);
    if (t->vao) glDeleteVertexArrays(1, &t->vao);
    if (t->prog) glDeleteProgram(t->prog);
    memset(t, 0, sizeof(*t));
}

/* ------------------------------------------------------------------ */
/* Point management                                                    */
/* ------------------------------------------------------------------ */

void trail_push(Trail* t, vec3 world_pos) {
    t->points[t->head] = world_pos;
    t->head = (t->head + 1) % TRAIL_MAX_POINTS;
    if (t->count < TRAIL_MAX_POINTS) t->count++;
}

void trail_reset(Trail* t) {
    t->head = 0;
    t->count = 0;
}

/* Return the i-th most recent point, i=0 being the newest. */
static vec3 trail_get(const Trail* t, int i) {
    int idx = (t->head - 1 - i + TRAIL_MAX_POINTS * 2) % TRAIL_MAX_POINTS;
    return t->points[idx];
}

/* ------------------------------------------------------------------ */
/* Render                                                              */
/* ------------------------------------------------------------------ */

void trail_render(Trail* t, mat4 viewproj, vec3 cam_pos, float opacity) {
    {
        static int dbg = 0;
        if (dbg++ % 60 == 0) {
            printf("[trail_render] count=%d prog=%u opacity=%.3f\n",
                   t->count, t->prog, opacity);
        }
    }
    {
        static int rr = 0;
        if (rr++ < 3) printf("[render] t=%p prog=%u count=%d\n",
                             (void*)t, t->prog, t->count);
    }
    static int dbg = 0;
    if (dbg++ % 60 == 0) printf("[trail] count=%d\n", t->count);

    if (t->count < 4) return;

    /* Build a quad strip: for each consecutive pair of points, emit
       4 vertices (bl, br, tr, tl in strip order). The camera-facing
       billboard axis is computed on the CPU and baked into the vertex
       positions, so the vertex shader is trivial. */

    int n = t->count;
    int quads = n - 1;
    if (quads > 4) quads = 4;

    float* verts = malloc(sizeof(float) * 16 * quads * 4);
    int v = 0;

    for (int i = 0; i < quads; ++i) {
        vec3 p0 = trail_get(t, i);       /* newer */
        vec3 p1 = trail_get(t, i + 1);   /* older */

        vec3 seg = v3_sub(p1, p0);
        float seg_len = v3_len(seg);
        if (seg_len < 1e-4f) continue;
        if (seg_len > 8.0f) continue;   /* skip wrap-around seam */
        vec3 seg_dir = v3_scale(seg, 1.0f / seg_len);

        /* Width axis = the jet's local up vector. Rolls with the jet. */
        extern vec3 game_jet_up(void);
        vec3 right = v3_norm(game_jet_up());
        (void)seg_dir;

        float age0 = (float)i       / (float)(n - 1);   /* 0 = newest */
        float age1 = (float)(i + 1) / (float)(n - 1);

        /* Width tapers with age: 1.0 at newest, 0.1 at oldest */
        /* DEBUG: very thin, constant width. */
        float w0 = 0.05f;
        float w1 = 0.05f;

        vec3 a_l = v3_add(p0, v3_scale(right, -w0));
        vec3 a_r = v3_add(p0, v3_scale(right,  w0));
        vec3 b_l = v3_add(p1, v3_scale(right, -w1));
        vec3 b_r = v3_add(p1, v3_scale(right,  w1));

        /* Two triangles per quad: (a_l, a_r, b_r), (a_l, b_r, b_l). */
        float* d = verts + v;
        /* tri 1 */
        d[0]=a_l.x; d[1]=a_l.y; d[2]=a_l.z; d[3]=age0; d+=4;
        d[0]=a_r.x; d[1]=a_r.y; d[2]=a_r.z; d[3]=age0; d+=4;
        d[0]=b_r.x; d[1]=b_r.y; d[2]=b_r.z; d[3]=age1; d+=4;
        /* tri 2 */
        d[0]=a_l.x; d[1]=a_l.y; d[2]=a_l.z; d[3]=age0; d+=4;
        d[0]=b_r.x; d[1]=b_r.y; d[2]=b_r.z; d[3]=age1; d+=4;
        d[0]=b_l.x; d[1]=b_l.y; d[2]=b_l.z; d[3]=age1; d+=4;
        v += 24;
    }

    int tri_count = v / 4 / 3;
    if (tri_count == 0) { free(verts); return; }

    /* Bind the trail's VAO first so the array-buffer update lands in
       the correct VAO state. Otherwise WebGL2 raises INVALID_OPERATION. */
    glBindVertexArray(t->vao);
    glBindVertexArray(t->vao);
    glBindBuffer(GL_ARRAY_BUFFER, t->vbo);
    {
        static int vv = 0;
        if (vv++ < 3) {
            printf("[trail] tri_count=%d  v[0..7] = %.3f %.3f %.3f %.3f  %.3f %.3f %.3f %.3f\n",
                   tri_count, verts[0], verts[1], verts[2], verts[3],
                   verts[4], verts[5], verts[6], verts[7]);
        }
    }
    glBindVertexArray(t->vao);
    glBindBuffer(GL_ARRAY_BUFFER, t->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * v, verts);
    free(verts);

    glUseProgram(t->prog);
    glUniformMatrix4fv(t->u_viewproj, 1, GL_FALSE, viewproj.m);
    glUniform3f(t->u_campos, cam_pos.x, cam_pos.y, cam_pos.z);
    glUniform1f(t->u_opacity, opacity);
    glUniform1f(t->u_max_age, 1.0f);
    glUniform1i(t->u_tex, 0);

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    {
        static int dd = 0;
        if (dd++ % 60 == 0) printf("[trail] draw tri_count=%d\n", tri_count);
    }
    {
        static int dd = 0;
        if (dd++ < 5) {
            GLint cur_prog = 0;
            glGetIntegerv(GL_CURRENT_PROGRAM, &cur_prog);
            printf("[trail] pre-draw: prog=%u cur_prog=%d vao_bound_ok=? tri_count=%d err=0x%04X\n",
                   t->prog, cur_prog, tri_count, glGetError());
        }
    }
    glDrawArrays(GL_TRIANGLES, 0, tri_count * 3);
    {
        static int dd2 = 0;
        if (dd2++ < 5) {
            printf("[trail] post-draw: err=0x%04X\n", glGetError());
        }
    }
    glBindVertexArray(0);

    /* Fully unbind trail state so the next pass starts clean. */
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glUseProgram(0);

    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDepthFunc(GL_LEQUAL);
}
