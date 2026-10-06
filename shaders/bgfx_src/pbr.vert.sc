$input a_position, a_normal, a_texcoord0, a_tangent
$output v_worldPos, v_normal, v_uv, v_tangent, v_bitangent, v_lightSpacePos

#include <bgfx_shader.sh>

uniform mat4 u_modelMat;
uniform mat4 u_engineViewProj;
uniform mat4 u_lightVPMat;
uniform mat4 u_normalMat;

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
