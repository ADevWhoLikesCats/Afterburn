#include <bgfx_shader.sh>

SAMPLER2D(s_sceneDepth, 0);
SAMPLER2D(s_shadowMap, 1);
uniform mat4 u_engineInvViewProj;
uniform mat4 u_lightVPMat;
uniform vec4 u_camPosEngine;
uniform vec4 u_lightDirEngine;
uniform vec4 u_lightColorEngine;
uniform vec4 u_lightIntensityEngine;
uniform vec4 u_fullResolution;
uniform vec4 u_halfResolution;
uniform vec4 u_fogDensity;
uniform vec4 u_fogHeightFall;
uniform vec4 u_fogMaxDist;
uniform vec4 u_fogSteps;
uniform vec4 u_sunScatter;
uniform vec4 u_ambientScatter;

vec3 world_from_depth(vec2 uv, float depth) {
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 world = mul(u_engineInvViewProj, ndc);
    return world.xyz / world.w;
}

float shadow_at(vec3 world_pos, float NdotL) {
    vec4 lightClip = mul(u_lightVPMat, vec4(world_pos, 1.0));
    vec3 proj = lightClip.xyz / lightClip.w;
    proj = proj * 0.5 + 0.5;
    if (proj.x < 0.0 || proj.x > 1.0 ||
        proj.y < 0.0 || proj.y > 1.0 ||
        proj.z > 1.0) return 1.0;
    float d = texture2D(s_shadowMap, proj.xy).r;
    float bias = max(0.002 * (1.0 - NdotL), 0.0008);
    return (proj.z - bias <= d) ? 1.0 : 0.0;
}

float hg_phase(float cos_theta, float g) {
    float g2 = g * g;
    float denom = 1.0 + g2 - 2.0 * g * cos_theta;
    return (1.0 - g2) / (4.0 * 3.14159265 * pow(denom, 1.5));
}

void main() {
    vec2 half_uv = gl_FragCoord.xy / u_halfResolution.xy;
    vec2 full_uv = half_uv;

    float depth = texture2D(s_sceneDepth, full_uv).r;

    float scene_dist;
    vec3 world;
    if (depth >= 0.9999) {
        scene_dist = u_fogMaxDist.x;
        world = u_camPosEngine.xyz;
    } else {
        world = world_from_depth(full_uv, depth);
        scene_dist = length(world - u_camPosEngine.xyz);
        scene_dist = min(scene_dist, u_fogMaxDist.x);
    }

    vec3 ray_dir = normalize(world - u_camPosEngine.xyz);
    if (depth >= 0.9999) {
        vec4 ndc = vec4(full_uv * 2.0 - 1.0, 1.0, 1.0);
        vec4 world_far = mul(u_engineInvViewProj, ndc);
        ray_dir = normalize(world_far.xyz / world_far.w - u_camPosEngine.xyz);
    }

    vec3 L = normalize(-u_lightDirEngine.xyz);
    float NdotL_view = max(dot(ray_dir, L), 0.0);

    float phase_sun     = hg_phase(dot(ray_dir, L), 0.6);
    float phase_ambient = 1.0 / (4.0 * 3.14159265);

    float step_size = scene_dist / u_fogSteps.x;

    vec3 accum_scatter   = vec3(0.0);
    float accum_transmit = 1.0;

    float jitter = fract(52.9829189 * fract(0.06711056 * gl_FragCoord.x +
                                            0.00583715 * gl_FragCoord.y));

    vec3 light_radiance = u_lightColorEngine.xyz * u_lightIntensityEngine.x;

    for (int i = 0; i < 128; ++i) {
        if (float(i) >= u_fogSteps.x) break;

        float t = (float(i) + jitter) * step_size;
        if (t > scene_dist) break;

        vec3 pos = u_camPosEngine.xyz + ray_dir * t;

        float heightFactor = exp(-u_fogHeightFall.x * max(pos.y, 0.0));
        float density = u_fogDensity.x * heightFactor;

        float shadow = shadow_at(pos, NdotL_view);

        vec3 scatter_sun     = light_radiance * phase_sun     * u_sunScatter.x     * shadow;
        vec3 scatter_ambient = light_radiance * phase_ambient * u_ambientScatter.x;

        vec3 scatter = (scatter_sun + scatter_ambient) * density * step_size;

        float extinction = density * step_size;
        accum_scatter += accum_transmit * scatter;
        accum_transmit *= exp(-extinction);
    }

    gl_FragColor = vec4(accum_scatter, accum_transmit);
}
