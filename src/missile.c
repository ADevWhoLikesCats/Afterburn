#include "missile.h"
#include "enemy.h"
#include "asset.h"
#include <GLES3/gl3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define METERS_PER_WORLD_UNIT 5.0f
#define WORLD_UNITS_PER_METER 0.2f
#define GRAVITY_MS2 9.81f

static vec3 rotate_axis_local(vec3 v, vec3 k, float rad) {
    float c = cosf(rad), s = sinf(rad);
    vec3 kv = v3_cross(k, v);
    float kd = v3_dot(k, v);
    return v3(
        v.x * c + kv.x * s + k.x * kd * (1.0f - c),
        v.y * c + kv.y * s + k.y * kd * (1.0f - c),
        v.z * c + kv.z * s + k.z * kd * (1.0f - c)
    );
}

static float clampf_local(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

void missile_bay_init(MissileBay* mb) {
    memset(mb, 0, sizeof(*mb));
    mb->track_remaining = MISSILE_MAX / 2;
    mb->burn_remaining  = MISSILE_MAX / 2;
}

static int spawn(MissileBay* mb, MissileKind kind,
                 vec3 pos, vec3 fwd, vec3 vel, int target_index)
{
    for (int i = 0; i < MISSILE_MAX; ++i) {
        Missile* m = &mb->missiles[i];
        if (m->active) continue;

        memset(m, 0, sizeof(*m));
        m->kind = kind;

        if (kind == MISSILE_KIND_TRACK) {
            m->mass_kg        = 85.0f;
            m->thrust_N       = 14000.0f;
            m->boost_duration = 3.0f;
            m->cd_area        = 0.018f;
            m->max_turn_rate  = 3.5f;
            m->max_range_wu   = 6000.0f / METERS_PER_WORLD_UNIT;
            m->target_index   = target_index;
        } else {   /* BURN */
            m->mass_kg        = 65.0f;
            m->thrust_N       = 70000.0f;
            m->boost_duration = 5.0f;
            m->cd_area        = 0.010f;
            m->max_turn_rate  = 0.0f;
            m->max_range_wu   = 20000.0f / METERS_PER_WORLD_UNIT;
            m->target_index   = -1;
        }

        /* Launch offset: 1 wu below, 0.5 wu to the right. */
        vec3 right = v3_norm(v3_cross(fwd, v3(0, 1, 0)));
        if (v3_len(right) < 1e-4f) right = v3(1, 0, 0);
        vec3 down = v3_norm(v3_cross(right, fwd));
        m->position = v3_add(pos, v3_add(v3_scale(right, -0.8f),
                                         v3_scale(down,  -0.5f)));

        /* Initial velocity: jet velocity + forward kick. */
        m->velocity = v3_add(vel, v3_scale(fwd, 30.0f));
        m->forward  = v3_norm(m->velocity);
        m->phase = MISSILE_PHASE_BOOST;
        m->phase_time = 0.0f;
        m->lifetime = 0.0f;
        m->lock_time = 0.0f;
        m->active = 1;
        return 1;
    }
    return 0;
}

void missile_fire(MissileBay* mb, MissileKind kind,
                  vec3 pos, vec3 fwd, vec3 vel, int target_index)
{
    if (mb->fire_cooldown > 0.0f) return;

    if (kind == MISSILE_KIND_TRACK) {
        if (mb->track_remaining <= 0) return;
        if (target_index < 0) return;   /* need a lock */
        if (spawn(mb, kind, pos, fwd, vel, target_index)) {
            mb->track_remaining--;
            mb->fire_cooldown = 0.5f;
        }
    } else {
        if (mb->burn_remaining <= 0) return;
        if (spawn(mb, kind, pos, fwd, vel, -1)) {
            mb->burn_remaining--;
            mb->fire_cooldown = 0.5f;
        }
    }
}

/* Simple pure-pursuit guidance with a max turn rate. */
static void guide_toward(Missile* m, vec3 target_pos, float dt) {
    if (m->max_turn_rate <= 0.0f) return;

    vec3 to_t = v3_sub(target_pos, m->position);
    float dist = v3_len(to_t);
    if (dist < 1e-4f) return;
    vec3 desired = v3_scale(to_t, 1.0f / dist);

    vec3 current = v3_norm(m->velocity);
    float cos_a = v3_dot(current, desired);
    if (cos_a > 0.99995f) return;
    float angle = acosf(clampf_local(cos_a, -1.0f, 1.0f));

    float max_turn = m->max_turn_rate * dt;
    float turn = (angle < max_turn) ? angle : max_turn;

    vec3 axis = v3_cross(current, desired);
    float axis_mag = v3_len(axis);
    if (axis_mag < 1e-6f) return;
    axis = v3_scale(axis, 1.0f / axis_mag);

    vec3 new_dir = rotate_axis_local(current, axis, turn);
    float speed = v3_len(m->velocity);
    m->velocity = v3_scale(new_dir, speed);
    m->forward  = new_dir;
}

static void integrate(Missile* m, float dt) {
    m->lifetime   += dt;
    m->phase_time += dt;

    if (m->phase == MISSILE_PHASE_BOOST && m->phase_time > m->boost_duration) {
        m->phase = MISSILE_PHASE_COAST;
        m->phase_time = 0.0f;
    }

    float thrust_N = (m->phase == MISSILE_PHASE_BOOST) ? m->thrust_N : 0.0f;

    float speed_wu = v3_len(m->velocity);
    float speed_ms = speed_wu * METERS_PER_WORLD_UNIT;
    float q = 0.5f * 1.225f * speed_ms * speed_ms;
    float drag_N = q * m->cd_area;

    vec3 vel_dir = (speed_wu > 0.01f)
                   ? v3_scale(m->velocity, 1.0f / speed_wu)
                   : m->forward;

    vec3 accel = v3(0, -GRAVITY_MS2, 0);
    accel = v3_add(accel, v3_scale(m->forward, thrust_N / m->mass_kg));
    accel = v3_add(accel, v3_scale(vel_dir, -drag_N / m->mass_kg));

    vec3 accel_wu = v3_scale(accel, WORLD_UNITS_PER_METER);
    m->velocity = v3_add(m->velocity, v3_scale(accel_wu, dt));
    m->position = v3_add(m->position, v3_scale(m->velocity, dt));

    if (m->position.y < 0.1f) m->active = 0;
}

static vec3 g_hit_pos[MISSILE_MAX];
static int  g_hit_target[MISSILE_MAX];
static int  g_hit_count = 0;

int missile_pop_hit(vec3* out_pos, int* out_target) {
    if (g_hit_count == 0) return 0;
    *out_pos = g_hit_pos[0];
    if (out_target) *out_target = g_hit_target[0];
    for (int i = 1; i < g_hit_count; ++i) {
        g_hit_pos[i-1]    = g_hit_pos[i];
        g_hit_target[i-1] = g_hit_target[i];
    }
    g_hit_count--;
    return 1;
}

void missile_update(MissileBay* mb, float dt) {
    if (mb->fire_cooldown > 0.0f) mb->fire_cooldown -= dt;
    if (mb->fire_cooldown < 0.0f) mb->fire_cooldown = 0.0f;

    for (int i = 0; i < MISSILE_MAX; ++i) {
        Missile* m = &mb->missiles[i];
        if (!m->active) continue;

        m->lock_time += dt;

        /* Guidance (arming delay of 0.3 s so the missile clears the wing). */
        if (m->target_index >= 0 && m->lock_time > 0.3f) {
            if (enemy_alive(m->target_index)) {
                guide_toward(m, enemy_position(m->target_index), dt);
            } else {
                m->target_index = -1;   /* lost lock, becomes ballistic */
            }
        }

        integrate(m, dt);

        /* Proximity detonation. */
        if (m->target_index >= 0 && enemy_alive(m->target_index)) {
            float d = v3_len(v3_sub(m->position, enemy_position(m->target_index)));
            if (d < MISSILE_PROX_FUSE) {
                if (g_hit_count < MISSILE_MAX) {
                    g_hit_pos[g_hit_count]    = m->position;
                    g_hit_target[g_hit_count] = m->target_index;
                    g_hit_count++;
                }
                enemy_kill(m->target_index);
                m->active = 0;
            }
        }

        if (m->lifetime > MISSILE_MAX_LIFE) m->active = 0;
    }
}

int missile_track_count(const MissileBay* mb) { return mb->track_remaining; }
int missile_burn_count(const MissileBay* mb)  { return mb->burn_remaining;  }

/* ---------- Render ---------- */

static GLuint g_prog = 0;
static GLint  u_viewproj = -1;
static GLuint g_vao = 0;
static GLuint g_vbo = 0;

static GLuint load_shader(GLenum type, const char* path) {
    size_t len = 0;
    char* src = asset_load(path, &len);
    if (!src) { fprintf(stderr, "missile: cannot load %s\n", path); abort(); }
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, (const GLchar* const*)&src, NULL);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        fprintf(stderr, "missile: compile failed [%s]:\n%s\n", path, log);
        abort();
    }
    free(src);
    return s;
}

static void ensure_render_init(void) {
    if (g_prog) return;
    GLuint vs = load_shader(GL_VERTEX_SHADER,   "/shaders/gun.vert");
    GLuint fs = load_shader(GL_FRAGMENT_SHADER, "/shaders/gun.frag");
    g_prog = glCreateProgram();
    glAttachShader(g_prog, vs);
    glAttachShader(g_prog, fs);
    glLinkProgram(g_prog);
    u_viewproj = glGetUniformLocation(g_prog, "uViewProj");
    glDeleteShader(vs);
    glDeleteShader(fs);

    glGenVertexArrays(1, &g_vao);
    glGenBuffers(1, &g_vbo);
    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
    glBindVertexArray(0);
}

void missile_render(MissileBay* mb, mat4 viewproj, vec3 cam) {
    (void)cam;
    ensure_render_init();

    static float verts[MISSILE_MAX * 6 * 3];
    int v_count = 0;

    for (int i = 0; i < MISSILE_MAX; ++i) {
        Missile* m = &mb->missiles[i];
        if (!m->active) continue;

        vec3 dir = v3_norm(m->velocity);
        vec3 up  = v3(0, 1, 0);
        if (fabsf(v3_dot(dir, up)) > 0.99f) up = v3(1, 0, 0);
        vec3 right = v3_norm(v3_cross(dir, up));

        vec3 tail = m->position;
        vec3 head = v3_add(m->position, v3_scale(dir, 1.2f));
        float w = 0.06f;

        vec3 a = v3_add(tail, v3_scale(right, -w));
        vec3 b = v3_add(tail, v3_scale(right,  w));
        vec3 c = v3_add(head, v3_scale(right,  w));
        vec3 d = v3_add(head, v3_scale(right, -w));

        verts[v_count*3+0]=a.x; verts[v_count*3+1]=a.y; verts[v_count*3+2]=a.z; v_count++;
        verts[v_count*3+0]=b.x; verts[v_count*3+1]=b.y; verts[v_count*3+2]=b.z; v_count++;
        verts[v_count*3+0]=c.x; verts[v_count*3+1]=c.y; verts[v_count*3+2]=c.z; v_count++;
        verts[v_count*3+0]=a.x; verts[v_count*3+1]=a.y; verts[v_count*3+2]=a.z; v_count++;
        verts[v_count*3+0]=c.x; verts[v_count*3+1]=c.y; verts[v_count*3+2]=c.z; v_count++;
        verts[v_count*3+0]=d.x; verts[v_count*3+1]=d.y; verts[v_count*3+2]=d.z; v_count++;
    }

    if (v_count == 0) return;

    glUseProgram(g_prog);
    glUniformMatrix4fv(u_viewproj, 1, GL_FALSE, viewproj.m);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * v_count, verts, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, v_count);

    glBindVertexArray(0);
    glUseProgram(0);
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}
