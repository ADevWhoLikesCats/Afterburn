#include <bgfx_shader.sh>
SAMPLER2D(s_source, 0);
uniform vec4 u_texel;

void main() {
    vec2 uv = gl_FragCoord.xy * u_texel.xy;
    vec3 c = vec3(0.0);
    c += texture2D(s_source, uv + vec2(-0.5, -0.5) * u_texel.xy * 2.0).rgb;
    c += texture2D(s_source, uv + vec2( 0.5, -0.5) * u_texel.xy * 2.0).rgb;
    c += texture2D(s_source, uv + vec2(-0.5,  0.5) * u_texel.xy * 2.0).rgb;
    c += texture2D(s_source, uv + vec2( 0.5,  0.5) * u_texel.xy * 2.0).rgb;
    gl_FragColor = vec4(c * 0.25, 1.0);
}
