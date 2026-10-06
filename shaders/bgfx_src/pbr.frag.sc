$input v_worldPos, v_normal, v_uv, v_tangent, v_bitangent, v_lightSpacePos

#include <bgfx_shader.sh>


SAMPLER2D(s_sh, 0);
SAMPLER2D(s_probeSGs, 1);
SAMPLER2D(s_ssrColor, 2);
SAMPLER2D(s_ssao, 3);
SAMPLER2D(s_gbufferAlbedo, 4);
SAMPLER2D(s_ssgi, 5);
SAMPLER2D(s_ssrDepth, 6);
SAMPLER2D(s_texBasecolor, 7);
SAMPLER2D(s_texMR, 8);
SAMPLER2D(s_texNormal, 9);
SAMPLER2D(s_texEmissive, 10);
SAMPLER2D(s_texOcclusion, 11);
SAMPLER2D(s_shadowMap, 12);

uniform vec4 u_camPosEngine;
uniform vec4 u_lightDirEngine;
uniform vec4 u_lightColorEngine;
uniform vec4 u_lightIntensityEngine;
uniform vec4 u_debugMode;
uniform vec4 u_sceneGridMin;
uniform vec4 u_sceneGridMax;
uniform mat4 u_engineViewProj;
uniform vec4 u_resolution;
uniform vec4 u_basecolorFactor;
uniform vec4 u_metallicFactor;
uniform vec4 u_roughnessFactor;
uniform vec4 u_normalScale;
uniform vec4 u_hasEmissive;
uniform vec4 u_emissiveFactor;
uniform vec4 u_emissiveStrength;
uniform vec4 u_hasBasecolor;
uniform vec4 u_hasMR;
uniform vec4 u_hasNormal;
uniform vec4 u_shadowEnabled;


const float PI = 3.14159265359;

void sh_basis(vec3 d, out float Y[9]) {
    Y[0] = 0.282095;
    Y[1] = 0.488603 * d.y;
    Y[2] = 0.488603 * d.z;
    Y[3] = 0.488603 * d.x;
    Y[4] = 1.092548 * d.x * d.y;
    Y[5] = 1.092548 * d.y * d.z;
    Y[6] = 0.315392 * (3.0 * d.z * d.z - 1.0);
    Y[7] = 1.092548 * d.x * d.z;
    Y[8] = 0.546274 * (d.x * d.x - d.y * d.y);
}

vec3 sh_irradiance(vec3 n) {
    float Y[9];
    sh_basis(n, Y);
    const float A0 = 3.14159265;
    const float A1 = 2.09439510;
    const float A2 = 0.78539816;
    vec3 E = vec3(0.0);
    for (int i = 0; i < 9; ++i) {
        vec2 sh_uv = vec2((float(i) + 0.5) / 9.0, 0.5);
        vec3 L = texture2DLod(s_sh, sh_uv, 0.0).rgb;
        float a = (i == 0) ? A0 : (i < 4) ? A1 : A2;
        E += L * Y[i] * a;
    }
    return max(E, vec3(0.0));
}

vec3 sg_specular_at_probe(vec3 R, float roughness, int probe_row) {
    vec3 accum = vec3(0.0);
    float rough2 = max(roughness * roughness, 1e-4);
    for (int i = 0; i < 32; ++i) {
        vec2 ax_uv = vec2((float(i*2+0) + 0.5) / 64.0, (float(probe_row) + 0.5) / 32.0);
        vec2 cl_uv = vec2((float(i*2+1) + 0.5) / 64.0, (float(probe_row) + 0.5) / 32.0);
        vec4 axis_sharp = texture2DLod(s_probeSGs, ax_uv, 0.0);
        vec4 color_w    = texture2DLod(s_probeSGs, cl_uv, 0.0);
        vec3  axis      = axis_sharp.xyz;
        float sharp     = axis_sharp.w;
        vec3  color     = color_w.rgb;
        float w         = color_w.a;

        float s = sharp / (1.0 + sharp * rough2 * 2.0);
        float k = exp(s * (dot(R, axis) - 1.0));
        accum += color * k * w;
    }
    return max(accum * 0.10, vec3(0.0));
}

vec3 sg_specular(vec3 worldPos, vec3 R, float roughness) {
    vec3 span = max(u_sceneGridMax.xyz - u_sceneGridMin.xyz, vec3(1e-4));
    vec3 t = (worldPos - u_sceneGridMin.xyz) / span;
    t = clamp(t, vec3(0.0), vec3(0.999));
    int px = int(t.x * 8.0);
    int py = int(t.y * 4.0);
    int pz = int(t.z * 8.0);
    int row = pz * (8 * 4) + py * 8 + px;
    return sg_specular_at_probe(R, roughness, row);
}

float hard_shadow(vec4 lightSpacePos, float bias) {
    vec3 proj = lightSpacePos.xyz / lightSpacePos.w;
    proj = proj * 0.5 + 0.5;
    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0 || proj.z > 1.0) {
        return 1.0;
    }
    float d = texture2D(s_shadowMap, proj.xy).r;
    return (proj.z - bias <= d) ? 1.0 : 0.0;
}

float D_GGX(float NdotH, float a) {
    float a2 = a * a;
    float d  = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}
float G_SchlickGGX(float NdotV, float k) {
    return NdotV / (NdotV * (1.0 - k) + k);
}
float G_Smith(float NdotV, float NdotL, float rough) {
    float r = rough + 1.0;
    float k = (r * r) / 8.0;
    return G_SchlickGGX(NdotV, k) * G_SchlickGGX(NdotL, k);
}
vec3 F_Schlick(vec3 F0, float VdotH) {
    return F0 + (vec3(1.0) - F0) * pow(1.0 - VdotH, 5.0);
}
vec3 evalSpecular(vec3 F0, float rough, float NdotV, float NdotL, float NdotH, float VdotH) {
    float a = rough * rough;
    float D = D_GGX(NdotH, a);
    float G = G_Smith(NdotV, NdotL, rough);
    vec3  F = F_Schlick(F0, VdotH);
    return (D * G * F) / max(4.0 * NdotV * NdotL, 1e-4);
}

void main() {
    /* MAGENTA DEBUG */ gl_FragColor = vec4(1.0, 0.0, 1.0, 1.0); return;
    vec3 albedo = u_basecolorFactor.xyz;
    if (u_hasBasecolor.x > 0.5) albedo *= texture2D(s_texBasecolor, v_uv).rgb;

    float metallic = u_metallicFactor.x;
    float roughness = u_roughnessFactor.x;
    if (u_hasMR.x > 0.5) {
        vec4 mr = texture2D(s_texMR, v_uv);
        roughness *= mr.g;
        metallic  *= mr.b;
    }
    roughness = clamp(roughness, 0.04, 1.0);
    metallic  = clamp(metallic, 0.0, 1.0);

    vec3 emissive_radiance = vec3(0.0);
    if (u_hasEmissive.x > 0.5) {
        emissive_radiance = texture2D(s_texEmissive, v_uv).rgb * u_emissiveFactor.xyz * u_emissiveStrength.x;
    }

    vec3 N = normalize(v_normal);
    if (u_hasNormal.x > 0.5) {
        vec3 nmap = texture2D(s_texNormal, v_uv).rgb * 2.0 - 1.0;
        nmap.xy *= u_normalScale.x;
        mat3 TBN = mat3(normalize(v_tangent), normalize(v_bitangent), N);
        N = normalize(mul(TBN, nmap));
    }

    /* DEBUG: early return with normals before anything else runs */
    if (int(u_debugMode.x) == 7) {
        gl_FragColor = vec4(N * 0.5 + 0.5, 1.0);
        return;
    }

    vec3 V = normalize(u_camPosEngine.xyz - v_worldPos);
    vec3 L = normalize(-u_lightDirEngine.xyz);
    vec3 H = normalize(V + L);
    vec3 R = reflect(-V, N);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 1e-4);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 kD = (vec3(1.0) - F_Schlick(F0, VdotH)) * (1.0 - metallic);

    int dbg = int(u_debugMode.x);
    vec2 suv = gl_FragCoord.xy / u_resolution.xy;

    float shadow = 1.0;
    if (u_shadowEnabled.x > 0.5) {
        float bias = max(0.0015 * (1.0 - NdotL), 0.0005);
        shadow = hard_shadow(v_lightSpacePos, bias);
    }

    vec3 radiance = u_lightColorEngine.xyz * u_lightIntensityEngine.x;
    vec3 diffuse  = kD * albedo / PI * radiance;
    vec3 specular = evalSpecular(F0, roughness, NdotV, NdotL, NdotH, VdotH) * radiance;
    vec3 direct   = (diffuse + specular) * NdotL * shadow;

    vec3 E_ambient = sh_irradiance(N);
    vec3 indirect_diffuse = kD * albedo / PI * E_ambient;

    vec3 ssgi = texture2D(s_ssgi, suv).rgb;
    indirect_diffuse += kD * ssgi;

    vec3 sg_refl = sg_specular(v_worldPos, R, roughness);
    vec3 F_indirect = F_Schlick(F0, NdotV);
    float roughAtten = 1.0 - roughness * 0.6;
    float upness = abs(N.y);
    float ground_mask = 1.0 - smoothstep(0.5, 0.9, upness);
    vec3 indirect_specular = min(sg_refl * F_indirect * roughAtten * ground_mask, vec3(2.0));
    float grazing = smoothstep(0.02, 0.15, NdotV);
    indirect_specular *= grazing;

    float ao = texture2D(s_ssao, suv).r;

    vec3 color = direct + (indirect_diffuse + indirect_specular) * ao + emissive_radiance;

    /* Debug: only override the base color. G-buffer still writes correctly. */
    if (dbg == 1)      color = albedo / PI;
    else if (dbg == 2) color = specular;
    else if (dbg == 3) color = F_Schlick(F0, VdotH);
    else if (dbg == 4) color = vec3(roughness);
    else if (dbg == 5) color = vec3(metallic);
    else if (dbg == 6) color = sh_irradiance(N);
    else if (dbg == 7) color = N * 0.5 + 0.5;
    else if (dbg == 8) color = sg_specular(v_worldPos, R, roughness);
    else if (dbg == 9) color = R * 0.5 + 0.5;
    else if (dbg == 10) color = vec3(1.0 - shadow);
    else if (dbg == 11) {
        vec3 span = max(u_sceneGridMax.xyz - u_sceneGridMin.xyz, vec3(1e-4));
        color = clamp((v_worldPos - u_sceneGridMin.xyz) / span, 0.0, 1.0);
    }
    else if (dbg == 12) color = texture2D(s_ssrColor, suv).rgb;
    else if (dbg == 14) color = texture2D(s_gbufferAlbedo, suv).rgb;
    else if (dbg == 15) color = texture2D(s_ssgi, suv).rgb;

    gl_FragColor = vec4(color, 1.0);
    
    
    
}
