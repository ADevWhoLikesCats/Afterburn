#include <bgfx_shader.sh>

SAMPLER2D(s_octa, 0);
uniform vec4 u_octaSize;
uniform vec4 u_SGAxisSharp[32];

float texel_solid_angle(vec2 uv01, float N) {
    vec2 uv = uv01 * 2.0 - 1.0;
    float du = 2.0 / N;
    float r2 = dot(uv, uv);
    return (4.0 * du * du) / pow(1.0 + r2, 1.5);
}

vec3 oct_decode(vec2 uv) {
    uv = uv * 2.0 - 1.0;
    vec3 n = vec3(uv.x, uv.y, 1.0 - abs(uv.x) - abs(uv.y));
    float t = max(-n.z, 0.0);
    n.x += n.x >= 0.0 ? -t : t;
    n.y += n.y >= 0.0 ? -t : t;
    return normalize(n);
}

void main() {
    int x = int(gl_FragCoord.x);
    int sg = x >> 1;
    int parity = x & 1;

    if (parity == 0) {
        gl_FragColor = u_SGAxisSharp[sg];
        return;
    }

    vec3 axis = u_SGAxisSharp[sg].xyz;
    float sharpness = u_SGAxisSharp[sg].w;

    vec3 accum = vec3(0.0);
    float weight = 0.0;
    float N = u_octaSize.x;

    const int MAX_ITER = 256;
    for (int y = 0; y < MAX_ITER; ++y) {
        if (float(y) >= N) break;
        for (int xx = 0; xx < MAX_ITER; ++xx) {
            if (float(xx) >= N) break;
            vec2 uv = (vec2(float(xx), float(y)) + 0.5) / N;
            vec3 d = oct_decode(uv);
            vec3 col = texture2D(s_octa, uv).rgb;
            float dOmega = texel_solid_angle(uv, N);

            float kernel = exp(sharpness * (dot(d, axis) - 1.0));
            accum += col * kernel * dOmega;
            weight += kernel * dOmega;
        }
    }

    vec3 amplitude = (weight > 1e-6) ? accum / weight : vec3(0.0);
    gl_FragColor = vec4(amplitude, weight);
}
