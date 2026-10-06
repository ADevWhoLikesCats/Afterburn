#include <bgfx_shader.sh>
SAMPLER2D(s_ssao, 0);
uniform vec4 u_invResolution;

void main() {
    vec2 uv = gl_FragCoord.xy * u_invResolution.xy;
    vec2 texel = u_invResolution.xy;

    float sum = 0.0;
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
            sum += texture2D(s_ssao, uv + vec2(float(dx), float(dy)) * texel).r;
        }
    }
    float ao = sum / 25.0;
    gl_FragColor = vec4(ao, ao, ao, 1.0);
}
