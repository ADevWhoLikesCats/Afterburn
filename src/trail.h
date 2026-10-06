#ifndef TRAIL_H
#define TRAIL_H

#include <GLES3/gl3.h>
#include "math3d.h"

#define TRAIL_MAX_POINTS 256

typedef struct {
    /* Ring buffer of world-space points, most recent at head-1. */
    vec3   points[TRAIL_MAX_POINTS];
    int    head;          /* next slot to write */
    int    count;         /* number of valid points (0..TRAIL_MAX_POINTS) */

    /* GPU resources */
    GLuint vao;
    GLuint vbo;
    GLuint prog;
    GLint  u_viewproj;
    GLint  u_campos;
    GLint  u_tex;
    GLint  u_opacity;
    GLint  u_max_age;
} Trail;

/* One trail per wingtip. */
void  trail_create(Trail* t);
void  trail_destroy(Trail* t);

/* Push a new world-space point. Called once per frame while emitting. */
void  trail_push(Trail* t, vec3 world_pos);

/* Reset the trail (e.g., when the jet respawns or the effect is off). */
void  trail_reset(Trail* t);

/* Render the trail. Caller has already bound the target framebuffer and
   set up the PBR uniforms (view/proj). Call this inside the scene pass. */
void  trail_render(Trail* t, mat4 viewproj, vec3 cam_pos, float opacity);

#endif
