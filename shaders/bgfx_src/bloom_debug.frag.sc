#include <bgfx_shader.sh>
SAMPLER2D(s_bloom, 0);
uniform vec4 u_invSize;
uniform vec4 u_gain;

void main() {
    vec2 uv = gl_FragCoord.xy * u_invSize.xy;
    vec3 c = texture2D(s_bloom, uv).rgb * u_gain.x;
    gl_FragColor = vec4(c, 1.0);
}
