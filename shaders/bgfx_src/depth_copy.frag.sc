#include <bgfx_shader.sh>
SAMPLER2D(s_depth, 0);

void main() {
    ivec2 sz = textureSize(s_depth, 0);
    vec2 uv = gl_FragCoord.xy / vec2(float(sz.x), float(sz.y));
    gl_FragColor = vec4(texture2D(s_depth, uv).r, 0.0, 0.0, 1.0);
}
