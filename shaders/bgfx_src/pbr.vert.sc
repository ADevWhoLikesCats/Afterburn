#include <bgfx_shader.sh>

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texcoord0;
layout(location = 3) in vec4 a_tangent;

uniform mat4 u_modelMat;
uniform mat4 u_engineViewProj;
uniform mat4 u_lightVPMat;
uniform mat4 u_normalMat;

out vec3 v_worldPos;
out vec3 v_normal;
out vec2 v_uv;
out vec3 v_tangent;
out vec3 v_bitangent;
out vec4 v_lightSpacePos;

void main() {
    vec4 worldPos = mul(u_modelMat, vec4(a_position, 1.0));

    v_worldPos = worldPos.xyz;
    v_normal    = normalize(mul(u_normalMat, vec4(a_normal, 0.0)).xyz);
    v_tangent   = normalize(mul(u_normalMat, vec4(a_tangent.xyz, 0.0)).xyz);
    v_bitangent = cross(v_normal, v_tangent) * a_tangent.w;
    v_uv = a_texcoord0;
    v_lightSpacePos = mul(u_lightVPMat, worldPos);

    gl_Position = mul(u_engineViewProj, worldPos);
}
