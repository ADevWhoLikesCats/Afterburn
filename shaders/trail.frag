#version 300 es
precision highp float;

in float vAge;

uniform float uOpacity;
uniform float uMaxAge;

out vec4 fragColor;

void main() {
    float t = clamp(vAge / uMaxAge, 0.0, 1.0);

    /* Smooth fade from wispy at the wingtip to invisible at the tail.
       The wingtip alpha is uOpacity (0.35 → ghost-like).
       The tail alpha is 0. */
    float fade = 1.0 - smoothstep(0.0, 1.0, t);

    /* Soft cool grey, not white — reads as smoke, not paint. */
    vec3 color = vec3(0.85, 0.87, 0.92);

    fragColor = vec4(color, fade * uOpacity);
}
