#include "enemy.h"
#include "game.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* The enemy reuses the player's jet meshes. game_jet_meshes() returns
   the same array, and g_jet_base_transform centers/scales the model. */
extern mat4 g_jet_base_transform;   /* defined in game.c, non-static */

static Enemy g_enemies[ENEMY_COUNT];
static float g_time = 0.0f;

/* PBR program from main.c */
extern GLuint game_pbr_prog(void);

static float clampf_local(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

void enemy_init_all(void) {
    memset(g_enemies, 0, sizeof(g_enemies));
    for (int i = 0; i < ENEMY_COUNT; ++i) {
        Enemy* e = &g_enemies[i];
        e->alive   = 1;
        e->phase   = (float)i * 3.14159f;          /* 180° apart */
        e->omega   = 0.28f + 0.06f * (float)i;     /* slightly different speeds */
        e->speed_wu = 50.0f;                       /* 250 m/s */

        /* Initial position on the Lissajous curve. */
        float t = e->phase;
        float A = 900.0f, B = 450.0f;
        e->position = v3(A * sinf(e->omega * t),
                         350.0f + 60.0f * sinf(2.0f * e->omega * t),
                         B * sinf(2.0f * e->omega * t));
        e->forward  = v3(1, 0, 0);
        e->up       = v3(0, 1, 0);
        e->right    = v3(0, 0, -1);
        e->velocity = v3(0, 0, 0);

        printf("[enemy] spawn %d at (%.1f, %.1f, %.1f) omega=%.2f\n",
               i, e->position.x, e->position.y, e->position.z, e->omega);
    }
}

void enemy_update_all(float dt) {
    g_time += dt;
    for (int i = 0; i < ENEMY_COUNT; ++i) {
        Enemy* e = &g_enemies[i];
        if (!e->alive) continue;

        /* Lissajous in the XZ plane, gentle altitude bob. */
        float t = g_time + e->phase;
        float A = 900.0f;
        float B = 450.0f;
        float w = e->omega;

        vec3 new_pos = v3(A * sinf(w * t),
                          350.0f + 60.0f * sinf(2.0f * w * t),
                          B * sinf(2.0f * w * t));

        /* Numerical velocity for missile lead. */
        if (dt > 1e-5f) {
            e->velocity = v3_scale(v3_sub(new_pos, e->position), 1.0f / dt);
        }
        e->position = new_pos;

        /* Nose follows velocity. */
        float vlen = v3_len(e->velocity);
        if (vlen > 1e-4f) {
            e->forward = v3_scale(e->velocity, 1.0f / vlen);
        }

        /* Keep the wings level: build up/right from world-up. */
        vec3 world_up = v3(0, 1, 0);
        vec3 r = v3_cross(e->forward, world_up);
        float rlen = v3_len(r);
        if (rlen > 1e-4f) {
            e->right = v3_scale(r, 1.0f / rlen);
            e->up    = v3_cross(e->right, e->forward);
        }
    }
}

int enemy_count_alive(void) {
    int n = 0;
    for (int i = 0; i < ENEMY_COUNT; ++i) if (g_enemies[i].alive) n++;
    return n;
}

vec3 enemy_position(int i) { return g_enemies[i].position; }
vec3 enemy_velocity(int i) { return g_enemies[i].velocity; }
vec3 enemy_forward(int i)  { return g_enemies[i].forward; }
int  enemy_alive(int i)    { return g_enemies[i].alive; }

void enemy_kill(int i) {
    if (i < 0 || i >= ENEMY_COUNT) return;
    if (g_enemies[i].alive) {
        printf("[enemy] killed %d at (%.1f, %.1f, %.1f)\n",
               i, g_enemies[i].position.x,
               g_enemies[i].position.y, g_enemies[i].position.z);
    }
    g_enemies[i].alive = 0;
}

mat4 enemy_model_matrix(int i) {
    Enemy* e = &g_enemies[i];
    mat4 m;
    /* Same body-to-model mapping as build_aircraft_model():
       column 0 = -forward, column 1 = up, column 2 = -right. */
    m.m[0]  = -e->forward.x;  m.m[4]  = e->up.x;  m.m[8]  = -e->right.x;
    m.m[12] = e->position.x;
    m.m[1]  = -e->forward.y;  m.m[5]  = e->up.y;  m.m[9]  = -e->right.y;
    m.m[13] = e->position.y;
    m.m[2]  = -e->forward.z;  m.m[6]  = e->up.z;  m.m[10] = -e->right.z;
    m.m[14] = e->position.z;
    m.m[3]  = 0;              m.m[7]  = 0;        m.m[11] = 0;
    m.m[15] = 1;
    return m;
}

/* Draw all enemies using the same PBR program as the jet.
   Reuses the uniform locations fetched in game_render. */
void enemy_render_all(int w, int h) {
    (void)w; (void)h;
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

    for (int e = 0; e < ENEMY_COUNT; ++e) {
        if (!g_enemies[e].alive) continue;

        mat4 placement = enemy_model_matrix(e);
        Mesh* meshes = game_jet_meshes();
        int   count  = game_jet_mesh_count();

        for (int i = 0; i < count; ++i) {
            Mesh* m = &meshes[i];
            mat4 world = m4_mul(placement, g_jet_base_transform);
            /* Re-apply the model's own per-mesh transform (identity here,
               but kept for parity with the jet path). */
            world = m4_mul(world, m->model);

            float nrm[9];
            m4_to_mat3_normal(world, nrm);
            glUniformMatrix4fv(u_model, 1, GL_FALSE, world.m);
            glUniformMatrix3fv(u_normalmat, 1, GL_FALSE, nrm);

            /* Tint the enemy red so it's distinguishable. */
            glUniform3f(u_basecolor_factor,
                        m->material.basecolor_factor.x * 0.9f,
                        m->material.basecolor_factor.y * 0.35f,
                        m->material.basecolor_factor.z * 0.35f);
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
            GLuint icount = m->index_count & 0x7FFFFFFF;
            GLenum idx_type = use_16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
            glDrawElements(GL_TRIANGLES, icount, idx_type, 0);
        }
    }
}
