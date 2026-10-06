#include <bgfx_shader.sh>

SAMPLER2D(s_currentColor, 0);
SAMPLER2D(s_historyColor, 1);
SAMPLER2D(s_currentDepth, 2);
uniform mat4 u_engineInvViewProj;
uniform mat4 u_prevViewProjMat;
uniform vec4 u_camPosEngine;
uniform vec4 u_resolution;
uniform vec4 u_blendFactor;

vec3 world_from_depth(vec2 uv, float depth) {
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 world = mul(u_engineInvViewProj, ndc);
    return world.xyz / world.w;
}

vec3 clip_to_neighborhood(vec3 history, vec2 uv, vec2 texel) {
    vec3 cmin = vec3(1e30);
    vec3 cmax = vec3(-1e30);
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            vec3 c = texture2D(s_currentColor, uv + vec2(float(dx), float(dy)) * texel).rgb;
            cmin = min(cmin, c);
            cmax = max(cmax, c);
        }
    }
    vec3 range = (cmax - cmin) * 1.5;
    vec3 mid = (cmax + cmin) * 0.5;
    return clamp(history, mid - range * 0.5, mid + range * 0.5);
}

void main() {
    vec2 uv = gl_FragCoord.xy / u_resolution.xy;
    vec2 texel = 1.0 / u_resolution.xy;

    float depth = texture2D(s_currentDepth, uv).r;

    if (depth >= 0.9999) {
        gl_FragColor = vec4(texture2D(s_currentColor, uv).rgb, 1.0);
        return;
    }

    vec3 world_pos = world_from_depth(uv, depth);

    vec4 prev_clip = mul(u_prevViewProjMat, vec4(world_pos, 1.0));
    if (prev_clip.w <= 0.0) {
        gl_FragColor = vec4(texture2D(s_currentColor, uv).rgb, 1.0);
        return;
    }
    vec2 prev_uv = prev_clip.xy / prev_clip.w * 0.5 + 0.5;

    if (prev_uv.x < 0.0 || prev_uv.x > 1.0 || prev_uv.y < 0.0 || prev_uv.y > 1.0) {
        gl_FragColor = vec4(texture2D(s_currentColor, uv).rgb, 1.0);
        return;
    }

    vec3 current = texture2D(s_currentColor, uv).rgb;
    vec3 history = texture2D(s_historyColor, prev_uv).rgb;

    history = clip_to_neighborhood(history, uv, texel);

    vec2 vel = abs(uv - prev_uv) * u_resolution.xy;
    float velocity_mag = length(vel);
    float blend = u_blendFactor.x;
    if (velocity_mag > 8.0) {
        blend = max(0.0, blend - (velocity_mag - 8.0) * 0.05);
    }

    gl_FragColor = vec4(mix(current, history, blend), 1.0);
}
