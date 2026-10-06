#include <bgfx_shader.sh>

uniform vec4 u_probePos;
uniform vec4 u_lightDirEngine;
uniform vec4 u_lightColorEngine;
uniform vec4 u_lightIntensityEngine;
uniform vec4 u_camPosEngine;
uniform vec4 u_invResolution;

const float PI = 3.14159265359;

vec3 oct_decode(vec2 uv) {
    uv = uv * 2.0 - 1.0;
    vec3 n = vec3(uv.x, uv.y, 1.0 - abs(uv.x) - abs(uv.y));
    float t = max(-n.z, 0.0);
    n.x += n.x >= 0.0 ? -t : t;
    n.y += n.y >= 0.0 ? -t : t;
    return normalize(n);
}

float intersect_cube(vec3 ro, vec3 rd, out vec3 outNormal) {
    vec3 inv = 1.0 / rd;
    vec3 t0 = (vec3(-0.5) - ro) * inv;
    vec3 t1 = (vec3( 0.5) - ro) * inv;
    vec3 tmin = min(t0, t1);
    vec3 tmax = max(t0, t1);
    float tNear = max(max(tmin.x, tmin.y), tmin.z);
    float tFar  = min(min(tmax.x, tmax.y), tmax.z);
    if (tFar < max(tNear, 0.0)) return -1.0;
    float t = tNear > 0.0 ? tNear : tFar;

    if (tmin.x >= tmin.y && tmin.x >= tmin.z) {
        outNormal = vec3(rd.x < 0.0 ? -1.0 : 1.0, 0.0, 0.0);
    } else if (tmin.y >= tmin.z) {
        outNormal = vec3(0.0, rd.y < 0.0 ? -1.0 : 1.0, 0.0);
    } else {
        outNormal = vec3(0.0, 0.0, rd.z < 0.0 ? -1.0 : 1.0);
    }
    return t;
}

vec3 face_color(vec3 n) {
    if (n.z >  0.9) return vec3(1, 0, 0);
    if (n.z < -0.9) return vec3(0, 1, 1);
    if (n.x >  0.9) return vec3(0, 1, 0);
    if (n.x < -0.9) return vec3(1, 0, 1);
    if (n.y >  0.9) return vec3(0, 0, 1);
    return vec3(1, 1, 0);
}

void main() {
    vec2 uv = gl_FragCoord.xy * u_invResolution.xy;
    vec3 rd = oct_decode(uv);
    vec3 ro = u_probePos.xyz;

    vec3 n;
    float t = intersect_cube(ro, rd, n);

    vec3 radiance;
    if (t < 0.0) {
        float y = rd.y * 0.5 + 0.5;
        radiance = mix(vec3(0.05, 0.06, 0.10), vec3(0.35, 0.45, 0.65), y);
    } else {
        vec3 albedo = face_color(n);
        vec3 L = normalize(-u_lightDirEngine.xyz);
        float NdotL = max(dot(n, L), 0.0);
        vec3 diffuse = albedo / PI * u_lightColorEngine.xyz * u_lightIntensityEngine.x * NdotL;
        vec3 ambient = albedo * 0.05;
        radiance = diffuse + ambient;

        vec3 V = -rd;
        float fres = pow(1.0 - max(dot(n, V), 0.0), 5.0);
        radiance += u_lightColorEngine.xyz * u_lightIntensityEngine.x * 0.02 * fres;
    }

    gl_FragColor = vec4(radiance, 1.0);
}
