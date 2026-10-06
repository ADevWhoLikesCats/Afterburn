$input a_position

#include <bgfx_shader.sh>

uniform mat4 u_lightVP;
uniform mat4 u_modelMat;

void main() {
    gl_Position = mul(u_lightVP, mul(u_modelMat, vec4(a_position, 1.0)));
}
