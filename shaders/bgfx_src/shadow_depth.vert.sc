#include <bgfx_shader.sh>

layout(location = 0) in vec3 a_position;

uniform mat4 u_lightVP;
uniform mat4 u_modelMat;

void main() {
    gl_Position = mul(u_lightVP, mul(u_modelMat, vec4(a_position, 1.0)));
}
