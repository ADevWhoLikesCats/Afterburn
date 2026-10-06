#include <bgfx_shader.sh>
SAMPLER2D(s_source, 0);
uniform vec4 u_texel;

void main() {
    vec2 uv = gl_FragCoord.xy * u_texel.xy;
    vec3 small = texture2D(s_source, uv).rgb;
    gl_FragColor = vec4(small, 1.0);
}
