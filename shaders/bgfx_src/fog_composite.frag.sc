#include <bgfx_shader.sh>
SAMPLER2D(s_sceneColor, 0);
SAMPLER2D(s_fogTexture, 1);
uniform vec4 u_invResolution;

void main() {
    vec2 uv = gl_FragCoord.xy * u_invResolution.xy;
    vec3 scene = texture2D(s_sceneColor, uv).rgb;
    vec4 fog   = texture2D(s_fogTexture, uv);

    vec3 result = scene * fog.a + fog.rgb;
    gl_FragColor = vec4(result, 1.0);
}
