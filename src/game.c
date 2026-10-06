#include "game.h"
#include <math.h>
#include <stdio.h>
#include <emscripten.h>
#include <emscripten/html5.h>

typedef struct {
    vec3  position;
    vec3  forward;
    vec3  up;
    vec3  right;
    float speed;
    float length;
    float min_speed;
    float max_speed;
    float accel;
    float pitch_rate;   /* rad/sec at full mouse input */
    float roll_rate;    /* rad/sec at full mouse input */
    float rudder_rate;  /* rad/sec at full rudder key */
    float bank_to_yaw;  /* extra yaw coupling from bank, per rad of bank */
} Aircraft;

static Aircraft g_jet;
static vec3     g_probe_grid_offset = {0,0,0};
static mat4     g_jet_base_transform;
static Model    g_jet_model;
static int      g_jet_loaded = 0;

static vec3     g_cam_pos;
static vec3     g_cam_target;
static int      g_cam_initialized = 0;

static int g_key_w, g_key_s, g_key_a, g_key_d, g_key_r;
static int g_pointer_locked = 0;

/* Mouse state: absolute pixels + accumulated delta since last frame */
static double g_mouse_x = 0.0, g_mouse_y = 0.0;
static double g_mouse_dx = 0.0, g_mouse_dy = 0.0;
static int    g_mouse_have_last = 0;

/* Per-frame axis values in [-1, 1] derived from mouse deltas.
   These decay each frame because they only represent movement during the
   last frame — the mouse is a rate input, not a position input. */
static float g_pitch_axis = 0.0f;
static float g_roll_axis  = 0.0f;

/* Radians per pixel of mouse motion. Tune these. */
#define MOUSE_PITCH_SENS 0.0035f
#define MOUSE_ROLL_SENS  0.0055f

static mat4 build_aircraft_model(void);
static void compute_wingtips(void);

void game_key_down(const char* key) {
    switch (key[0]) {
        case 'w': case 'W': g_key_w = 1; break;
        case 's': case 'S': g_key_s = 1; break;
        case 'a': case 'A': g_key_a = 1; break;
        case 'd': case 'D': g_key_d = 1; break;
        case 'r': case 'R': g_key_r = 1; break;
    }
}
void game_key_up(const char* key) {
    switch (key[0]) {
        case 'w': case 'W': g_key_w = 0; break;
        case 's': case 'S': g_key_s = 0; break;
        case 'a': case 'A': g_key_a = 0; break;
        case 'd': case 'D': g_key_d = 0; break;
    }
}

void game_mouse_move(double dx, double dy) {
    if (!g_pointer_locked) return;
    g_mouse_dx += dx;
    g_mouse_dy += dy;
}

void game_set_pointer_locked(int locked) {
    g_pointer_locked = locked;
    g_mouse_have_last = 0;   /* reset baseline so the first move after lock doesn't jump */
    g_mouse_dx = g_mouse_dy = 0.0;
}

static vec3 rotate_axis(vec3 v, vec3 k, float rad) {
    float c = cosf(rad), s = sinf(rad);
    vec3 kv = v3_cross(k, v);
    float kd = v3_dot(k, v);
    return v3(
        v.x * c + kv.x * s + k.x * kd * (1.0f - c),
        v.y * c + kv.y * s + k.y * kd * (1.0f - c),
        v.z * c + kv.z * s + k.z * kd * (1.0f - c)
    );
}

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static vec3 lerp3(vec3 a, vec3 b, float t) {
    return v3(a.x + (b.x - a.x) * t,
              a.y + (b.y - a.y) * t,
              a.z + (b.z - a.z) * t);
}

void game_init(void) {
    printf("[game] loading jet model\n");
    g_jet_model = model_load_glb("/assets/model.glb");
    if (g_jet_model.count == 0) g_jet_model = model_load_glb("/assets/jet.glb");
    if (g_jet_model.count == 0) g_jet_model = model_load_glb("/assets/f16.glb");
    if (g_jet_model.count == 0) {
        fprintf(stderr, "[game] no jet model found\n");
        return;
    }

    vec3 c = model_center(&g_jet_model);
    vec3 e = model_extent(&g_jet_model);
    float longest = e.x;
    if (e.y > longest) longest = e.y;
    if (e.z > longest) longest = e.z;
    float s = (longest > 1e-6f) ? (3.0f / longest) : 1.0f;

    mat4 S = m4_scale(v3(s, s, s));
    mat4 T = m4_translate(v3(-c.x * s, -c.y * s, -c.z * s));
    mat4 jet_transform = m4_mul(T, S);
    g_jet_base_transform = jet_transform;
    for (int i = 0; i < g_jet_model.count; ++i) {
        g_jet_model.meshes[i].model = jet_transform;
    }

    g_jet.length       = 3.0f;
    g_jet.min_speed    = g_jet.length * 2.5f;
    g_jet.max_speed    = g_jet.length * 14.0f;
    g_jet.accel        = g_jet.length * 6.0f;

    /* Full-deflection rates */
    g_jet.pitch_rate   = 70.0f  * (3.14159265f / 180.0f);
    g_jet.roll_rate    = 150.0f * (3.14159265f / 180.0f);
    g_jet.rudder_rate  = 45.0f  * (3.14159265f / 180.0f);
    g_jet.bank_to_yaw  = 1.2f;

    g_jet.position = v3(0.0f, 4.0f, 0.0f);
    g_jet.forward  = v3(0.0f, 0.0f, -1.0f);
    g_jet.up       = v3(0.0f, 1.0f, 0.0f);
    g_jet.right    = v3(1.0f, 0.0f, 0.0f);
    g_jet.speed    = g_jet.length * 6.0f;

    g_jet_loaded = 1;
    compute_wingtips();
    printf("[game] jet loaded: length=%.2f\n", g_jet.length);
    g_probe_grid_offset = v3_sub(g_jet.position, v3(0.0f, 2.0f, 0.0f));
    printf("[game] probe grid offset initialized to (%.1f, %.1f, %.1f)\n",
           g_probe_grid_offset.x, g_probe_grid_offset.y, g_probe_grid_offset.z);
}

void game_update(float dt) {
    if (!g_jet_loaded || dt <= 0.0f) return;

    /* --- Convert mouse deltas to per-frame axis values --- */
    /* A single frame's mouse motion produces a small pitch/roll pulse.
       Held motion (mouse continuously moved in a direction) gives continuous
       rotation, which feels like a flight stick. */
    float pitch_mouse = (float)(-g_mouse_dy) * MOUSE_PITCH_SENS;   /* up = nose up */
    float roll_mouse  = (float)(-g_mouse_dx) * MOUSE_ROLL_SENS;    /* right = roll right */
    g_mouse_dx = g_mouse_dy = 0.0;

    /* Clamp per-frame contributions so a violent mouse flick can't snap the jet. */
    pitch_mouse = clampf(pitch_mouse, -0.5f, 0.5f);
    roll_mouse  = clampf(roll_mouse,  -0.5f, 0.5f);

    /* Smooth the axis a bit so a single frame of mouse input produces a
       smooth rotation instead of an instant snap. */
    g_pitch_axis = g_pitch_axis * 0.55f + pitch_mouse * 0.45f;
    g_roll_axis  = g_roll_axis  * 0.55f + roll_mouse  * 0.45f;

    /* --- Pitch (mouse vertical) --- */
    float pitch = g_pitch_axis * g_jet.pitch_rate * dt * 4.0f;
    g_jet.forward = rotate_axis(g_jet.forward, g_jet.right, pitch);
    g_jet.up      = rotate_axis(g_jet.up,      g_jet.right, pitch);

    /* --- Roll (mouse horizontal) --- */
    float roll = g_roll_axis * g_jet.roll_rate * dt * 4.0f;
    g_jet.up    = rotate_axis(g_jet.up,    g_jet.forward, roll);
    g_jet.right = rotate_axis(g_jet.right, g_jet.forward, roll);

    /* --- Rudder (A/D, around world up) --- */
    float rudder_in = (float)(g_key_a - g_key_d);
    float rudder = rudder_in * g_jet.rudder_rate * dt;
    g_jet.forward = rotate_axis(g_jet.forward, v3(0,1,0), rudder);
    g_jet.up      = rotate_axis(g_jet.up,      v3(0,1,0), rudder);
    g_jet.right   = rotate_axis(g_jet.right,   v3(0,1,0), rudder);

    /* --- Bank-to-turn: banked flight still yaws --- */
    float bank = asinf(clampf(g_jet.right.y, -1.0f, 1.0f));
    float yaw  = -bank * g_jet.bank_to_yaw * dt;
    g_jet.forward = rotate_axis(g_jet.forward, v3(0,1,0), yaw);
    g_jet.up      = rotate_axis(g_jet.up,      v3(0,1,0), yaw);
    g_jet.right   = rotate_axis(g_jet.right,   v3(0,1,0), yaw);

    /* --- Re-orthonormalize --- */
    g_jet.forward = v3_norm(g_jet.forward);
    g_jet.right   = v3_norm(v3_cross(g_jet.forward, g_jet.up));
    g_jet.up      = v3_cross(g_jet.right, g_jet.forward);

    /* --- Throttle (W = faster, S = slower) --- */
    float throttle = (float)(g_key_w - g_key_s);
    if (throttle > 0.0f) {
        g_jet.speed += g_jet.accel * dt;
    } else if (throttle < 0.0f) {
        g_jet.speed -= g_jet.accel * dt;
    } else {
        float cruise = g_jet.min_speed + 0.4f * (g_jet.max_speed - g_jet.min_speed);
        g_jet.speed += (cruise - g_jet.speed) * 0.4f * dt;
    }
    g_jet.speed = clampf(g_jet.speed, g_jet.min_speed, g_jet.max_speed);

    /* --- Position --- */
    g_jet.position = v3_add(g_jet.position,
                            v3_scale(g_jet.forward, g_jet.speed * dt));

    /* Keep the probe grid centered on the jet. The bake itself is
       local to the grid, so we only slide the world-to-grid
       mapping by the same amount. Reflections stay stable. */
    g_probe_grid_offset = v3_sub(g_jet.position, v3(0.0f, 2.0f, 0.0f));

    /* --- Chase camera --- */
    float speed_frac = clampf((g_jet.speed - g_jet.min_speed)
                              / (g_jet.max_speed - g_jet.min_speed), 0.0f, 1.0f);
    float dist_back = g_jet.length * (2.4f + 0.5f * speed_frac);
    float height_up = g_jet.length * (0.8f + 0.15f * speed_frac);

    vec3 desired_pos = v3_add(
        v3_add(g_jet.position, v3_scale(g_jet.forward, -dist_back)),
        v3(0.0f, height_up, 0.0f));
    vec3 desired_target = v3_add(g_jet.position,
                                 v3_scale(g_jet.forward, g_jet.length * 0.6f));

    if (!g_cam_initialized) {
        g_cam_pos = desired_pos;
        g_cam_target = desired_target;
        g_cam_initialized = 1;
    } else {
        float pos_k = 1.0f - expf(-6.0f * dt);
        float aim_k = 1.0f - expf(-10.0f * dt);
        g_cam_pos    = lerp3(g_cam_pos,    desired_pos,    pos_k);
        g_cam_target = lerp3(g_cam_target, desired_target, aim_k);
    }

    if (g_key_r) {
        g_jet.position = v3(0.0f, 4.0f, 0.0f);
        g_jet.forward  = v3(0.0f, 0.0f, -1.0f);
        g_jet.up       = v3(0.0f, 1.0f, 0.0f);
        g_jet.right    = v3(1.0f, 0.0f, 0.0f);
        g_jet.speed    = g_jet.length * 6.0f;
        g_cam_initialized = 0;
        g_key_r = 0;
    }
}

void game_prepare_transform(void) {
    if (!g_jet_loaded) return;
    mat4 placement = build_aircraft_model();
    for (int i = 0; i < g_jet_model.count; ++i) {
        g_jet_model.meshes[i].model = m4_mul(placement, g_jet_base_transform);
    }
}

Mesh* game_jet_meshes(void)    { return g_jet_model.meshes; }
int   game_jet_mesh_count(void){ return g_jet_loaded ? g_jet_model.count : 0; }

/* ------------------------------------------------------------------ */
/* Wingtip positions                                                   */
/* ------------------------------------------------------------------ */

static vec3 g_wingtip_left_local  = { 0, 0, 0 };
static vec3 g_wingtip_right_local = { 0, 0, 0 };
static vec3 g_wingtip_left_world  = { 0, 0, 0 };
static vec3 g_wingtip_right_world = { 0, 0, 0 };
static int  g_wingtips_initialized = 0;

static vec3 xform_point(mat4 m, vec3 p) {
    return v3(
        m.m[0]*p.x + m.m[4]*p.y + m.m[8]*p.z  + m.m[12],
        m.m[1]*p.x + m.m[5]*p.y + m.m[9]*p.z  + m.m[13],
        m.m[2]*p.x + m.m[6]*p.y + m.m[10]*p.z + m.m[14]
    );
}

/* Compute the local-space wingtip offsets from the model's bounding box.
   Assumes wingspan is along Z (F-16 model: nose along -X, up along +Y).
   The offsets are in the model's local space, before auto-scaling. */
static void compute_wingtips(void) {
    vec3 e = model_extent(&g_jet_model);
    vec3 c = model_center(&g_jet_model);

    float half_span = e.z * 0.5f;
    float wing_y    = c.y - e.y * 0.05f;   /* slight bias downward */
    float wing_x    = c.x;

    g_wingtip_left_local  = v3(wing_x, wing_y, c.z - half_span);
    g_wingtip_right_local = v3(wing_x, wing_y, c.z + half_span);
    g_wingtips_initialized = 1;

    printf("[wingtip] bbox extent=(%.3f,%.3f,%.3f) center=(%.3f,%.3f,%.3f)\n",
           e.x, e.y, e.z, c.x, c.y, c.z);
    printf("[wingtip] local L=(%.3f,%.3f,%.3f) R=(%.3f,%.3f,%.3f)\n",
           g_wingtip_left_local.x,  g_wingtip_left_local.y,  g_wingtip_left_local.z,
           g_wingtip_right_local.x, g_wingtip_right_local.y, g_wingtip_right_local.z);
}

void game_update_wingtips(void) {
    if (!g_wingtips_initialized) return;
    /* Same transform the renderer uses to place the jet's meshes. */
    mat4 placement = m4_mul(build_aircraft_model(), g_jet_base_transform);
    g_wingtip_left_world  = xform_point(placement, g_wingtip_left_local);
    g_wingtip_right_world = xform_point(placement, g_wingtip_right_local);
}

vec3 game_wingtip_left(void)  { return g_wingtip_left_world; }
vec3 game_wingtip_right(void) { return g_wingtip_right_world; }

vec3  game_jet_position(void)  { return g_jet.position; }
vec3  game_jet_right(void)     { return g_jet.right; }
vec3  game_jet_up(void)        { return g_jet.up; }

vec3 game_probe_grid_offset(void) { return g_probe_grid_offset; }
vec3 game_camera_pos(void)    { return g_cam_pos; }
vec3 game_camera_target(void) { return g_cam_target; }
int  game_ready(void)         { return g_jet_loaded; }

static mat4 build_aircraft_model(void) {
    mat4 m;
    vec3 f = g_jet.forward;
    vec3 u = g_jet.up;
    vec3 r = g_jet.right;

    m.m[0]  = -f.x;  m.m[4]  = u.x;  m.m[8]  = -r.x;  m.m[12] = g_jet.position.x;
    m.m[1]  = -f.y;  m.m[5]  = u.y;  m.m[9]  = -r.y;  m.m[13] = g_jet.position.y;
    m.m[2]  = -f.z;  m.m[6]  = u.z;  m.m[10] = -r.z;  m.m[14] = g_jet.position.z;
    m.m[3]  =  0;    m.m[7]  = 0;    m.m[11] =  0;    m.m[15] = 1;
    return m;
}

extern GLuint game_pbr_prog(void);

void game_render(int w, int h) {
    (void)w; (void)h;
    if (!g_jet_loaded) return;

    mat4 placement = build_aircraft_model();
    GLuint prog = game_pbr_prog();
    if (!prog) return;

    GLint u_model            = glGetUniformLocation(prog, "uModel");
    GLint u_normalmat        = glGetUniformLocation(prog, "uNormalMat");
    GLint u_basecolor_factor = glGetUniformLocation(prog, "uBasecolorFactor");
    GLint u_metallic_factor  = glGetUniformLocation(prog, "uMetallicFactor");
    GLint u_roughness_factor = glGetUniformLocation(prog, "uRoughnessFactor");
    GLint u_normal_scale     = glGetUniformLocation(prog, "uNormalScale");
    GLint u_has_basecolor    = glGetUniformLocation(prog, "uHasBasecolor");
    GLint u_has_mr           = glGetUniformLocation(prog, "uHasMR");
    GLint u_has_normal       = glGetUniformLocation(prog, "uHasNormal");

    for (int i = 0; i < g_jet_model.count; ++i) {
        Mesh* m = &g_jet_model.meshes[i];
        mat4 world = m->model;

        float nrm[9];
        m4_to_mat3_normal(world, nrm);
        glUniformMatrix4fv(u_model, 1, GL_FALSE, world.m);
        glUniformMatrix3fv(u_normalmat, 1, GL_FALSE, nrm);

        glUniform3f(u_basecolor_factor,
                    m->material.basecolor_factor.x,
                    m->material.basecolor_factor.y,
                    m->material.basecolor_factor.z);
        glUniform1f(u_metallic_factor,  m->material.metallic_factor);
        glUniform1f(u_roughness_factor, m->material.roughness_factor);
        glUniform1f(u_normal_scale,     m->material.normal_scale);
        glUniform1i(u_has_basecolor,    m->material.has_basecolor);
        glUniform1i(u_has_mr,           m->material.has_mr);
        glUniform1i(u_has_normal,       m->material.has_normal);

        glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, m->material.tex_basecolor);
        glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, m->material.tex_mr);
        glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, m->material.tex_normal);
        glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_2D, m->material.tex_emissive);
        glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, m->material.tex_occlusion);
        glActiveTexture(GL_TEXTURE0);

        if (m->material.double_sided) glDisable(GL_CULL_FACE);
        else glEnable(GL_CULL_FACE);

        glBindVertexArray(m->vao);
        int use_16 = (m->index_count & 0x80000000) != 0;
        GLuint count = m->index_count & 0x7FFFFFFF;
        GLenum idx_type = use_16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
        glDrawElements(GL_TRIANGLES, count, idx_type, 0);
    }
}
