#include <bgfx_shader.sh>

SAMPLER2D(s_sceneColor, 0);
SAMPLER2D(s_sceneDepth, 1);
SAMPLER2D(s_gbufferAlbedo, 2);
SAMPLER2D(s_gbufferNormal, 3);
SAMPLER2D(s_gbufferEmissive, 4);
uniform mat4 u_engineInvViewProj;
uniform mat4 u_engineViewProj;
uniform vec4 u_camPosEngine;
uniform vec4 u_resolution;

vec3 world_from_depth(vec2 uv, float depth) {
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 world = mul(u_engineInvViewProj, ndc);
    return world.xyz / world.w;
}

vec3 normal_from_depth(vec2 uv, float depth) {
    vec2 texel = 1.0 / u_resolution.xy;
    vec3 p  = world_from_depth(uv, depth);
    vec3 px = world_from_depth(uv + vec2(texel.x, 0.0),
                               texture2D(s_sceneDepth, uv + vec2(texel.x, 0.0)).r);
    vec3 py = world_from_depth(uv + vec2(0.0, texel.y),
                               texture2D(s_sceneDepth, uv + vec2(0.0, texel.y)).r);
    return normalize(cross(px - p, py - p));
}

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main() {
    vec2 uv = gl_FragCoord.xy / u_resolution.xy;
    float depth = texture2D(s_sceneDepth, uv).r;

    if (depth >= 0.9999) {
        gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec3 world_pos = world_from_depth(uv, depth);
    vec3 N = normal_from_depth(uv, depth);
    vec3 V = normalize(u_camPosEngine.xyz - world_pos);

    vec3 up = abs(N.y) < 0.99 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 T = normalize(cross(up, N));
    vec3 B = cross(N, T);

    float radius_px = 24.0;

    const int SAMPLES = 8;
    vec3 accum = vec3(0.0);

    float jitter = hash(gl_FragCoord.xy);

    for (int i = 0; i < SAMPLES; ++i) {
        float fi = float(i);
        float phi = (fi + jitter) * 2.39996323;
        float cosTheta = sqrt(1.0 - (fi + 0.5) / float(SAMPLES));
        float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
        vec3 dir_ts = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);

        vec3 dir = normalize(T * dir_ts.x + B * dir_ts.y + N * dir_ts.z);

        vec3 target = world_pos + dir * (radius_px * 0.04);
        vec4 clip = mul(u_engineViewProj, vec4(target, 1.0));
        if (clip.w <= 0.0) continue;
        vec3 ndc = clip.xyz / clip.w;
        vec2 sample_uv = ndc.xy * 0.5 + 0.5;
        if (sample_uv.x < 0.0 || sample_uv.x > 1.0 ||
            sample_uv.y < 0.0 || sample_uv.y > 1.0) continue;

        float sample_depth = texture2D(s_sceneDepth, sample_uv).r;
        if (sample_depth >= 0.9999) continue;

        vec3 sample_world = world_from_depth(sample_uv, sample_depth);
        vec3 sample_normal = normal_from_depth(sample_uv, sample_depth);

        vec3 to_sample = sample_world - world_pos;
        float dist = length(to_sample);
        if (dist < 0.001) continue;
        vec3 L = to_sample / dist;

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL <= 0.0) continue;

        vec3 sample_lit  = texture2D(s_sceneColor, sample_uv).rgb;
        vec3 sample_emis = texture2D(s_gbufferEmissive, sample_uv).rgb;
        vec3 radiance = sample_lit + sample_emis;

        float atten = 1.0 / (1.0 + dist * dist);

        accum += radiance * NdotL * atten;
    }

    accum *= (1.0 / float(SAMPLES)) * 0.5;

    gl_FragColor = vec4(accum, 1.0);
}
