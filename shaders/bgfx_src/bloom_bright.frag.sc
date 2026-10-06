#include <bgfx_shader.sh>

SAMPLER2D(s_scene, 0);
uniform vec4 u_texel;
uniform vec4 u_threshold;

void main() {
    vec2 uv = gl_FragCoord.xy * u_texel.xy;
    vec3 c = texture2D(s_scene, uv).rgb;

    float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));
    float soft = lum - u_threshold.x + 0.1;
    soft = clamp(soft, 0.0, 0.2);
    soft = soft * soft / 0.4;

    float contrib = max(soft, lum - u_threshold.x) / max(lum, 1e-5);
    vec3 bloom = c * contrib;

    /* Cap bloom contribution so a single bright pixel can't streak */
    bloom = min(bloom, vec3(5.0));

    gl_FragColor = vec4(bloom, 1.0);
}
