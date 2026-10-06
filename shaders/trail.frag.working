#version 300 es
precision highp float;

in float vAge;

uniform float uOpacity;
uniform float uMaxAge;

out vec4 fragColor;

void main() {
    float t = clamp(vAge / uMaxAge, 0.0, 1.0);

    /* Hold full opacity for the first 30% of the trail,
       then fade smoothly to zero by 85%. Past 85%, fully invisible. */
    float fade = 1.0 - smoothstep(0.3, 0.85, t);

    /* If you want NO trail at all past a certain point,
       clamp to zero explicitly. */
    if (t > 0.85) fade = 0.0;

    vec3 tip_color  = vec3(0.80, 0.82, 0.86);
    vec3 tail_color = vec3(0.60, 0.62, 0.66);
    vec3 color = mix(tip_color, tail_color, t);

    fragColor = vec4(color, fade * uOpacity);
}
