#include <bgfx_shader.sh>

uniform mat4 u_customInvViewProj;
uniform vec4 u_invResolution;
uniform vec4 u_hdriValid;
SAMPLER2D(s_hdri, 0);

vec3 aces(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x*(a*x + b)) / (x*(c*x + d) + e), 0.0, 1.0);
}

void main() {
    vec2 uv = gl_FragCoord.xy * u_invResolution.xy;
    vec4 ndc = vec4(uv * 2.0 - 1.0, 1.0, 1.0);
    vec4 world = mul(u_customInvViewProj, ndc);
    vec3 dir = normalize(world.xyz / world.w);

    vec3 col;

    if (u_hdriValid.x > 0.5) {
        float phi   = atan2(dir.z, dir.x);
        float theta = acos(clamp(dir.y, -1.0, 1.0));
        vec2  hdr_uv = vec2(phi / (2.0 * 3.14159265) + 0.5,
                            theta / 3.14159265);
        col = texture2D(s_hdri, hdr_uv).rgb;
    } else {
        float up = clamp(dir.y, -1.0, 1.0);
        vec3 zenith  = vec3(0.08, 0.20, 0.62);
        vec3 horizon = vec3(0.85, 0.72, 0.55);
        vec3 ground  = vec3(0.10, 0.09, 0.08);
        if (up >= 0.0) col = mix(horizon, zenith, pow(up, 0.35));
        else           col = mix(horizon, ground, pow(-up, 0.5));
    }

    vec3 sun_dir = normalize(vec3(0.4, 1.0, 0.5));
    float sun = max(dot(dir, sun_dir), 0.0);
    col += vec3(1.00, 0.95, 0.85) * pow(sun, 900.0) * 25.0;
    col += vec3(1.00, 0.80, 0.55) * pow(sun, 16.0)  * 0.5;

    col = aces(col * 0.8);

    gl_FragColor = vec4(col, 1.0);
}
