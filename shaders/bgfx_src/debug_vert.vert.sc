#include <bgfx_shader.sh>

void main() {
    /* Hard-coded fullscreen triangle, ignore any vertex buffer */
    vec2 pos = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
}
