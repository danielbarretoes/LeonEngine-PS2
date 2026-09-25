#version 330 core
in vec3 vWorldPos;
in vec2 vTexCoord;
in vec4 vLightSpacePos;
in mat3 vTBN;

#define MAX_DIR 2
#define MAX_POINT 4

layout(std140) uniform Camera {
    mat4 uView;
    mat4 uProjection;
    mat4 uViewProjection;
    vec4 uCameraPos; // xyz used
};

layout(std140) uniform Lights {
    int uDirCount;
    int uPointCount;
    int _lightsPad0;
    int _lightsPad1;
    vec4 uDirDirections[MAX_DIR];
    vec4 uDirColors[MAX_DIR];
    vec4 uPointPositions[MAX_POINT];
    vec4 uPointColors[MAX_POINT];
    vec4 uPointRanges[MAX_POINT]; // .x = range
};

uniform vec3 uAlbedo;
uniform vec3 uSpecular;
uniform float uMetallic;
uniform float uAlpha;
uniform float uShininess;
uniform float uRoughness;
uniform sampler2D uAlbedoMap;
uniform sampler2D uNormalMap;
uniform sampler2D uShadowMap;
uniform sampler2D uPlanarReflection;
uniform int uHasPlanarReflection;
uniform int uReceiveShadows;
uniform float uShadowTexelSize;
uniform mat4 uReflectionViewProj;
uniform vec2 uUvScale;

out vec4 FragColor;

float shadowFactor(vec4 lightSpacePos, vec3 N, vec3 L) {
    vec3 proj = lightSpacePos.xyz / lightSpacePos.w;
    proj = proj * 0.5 + 0.5;
    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0) {
        return 0.0;
    }

    float bias = max(0.004 * (1.0 - max(dot(N, L), 0.0)), 0.0015);
    float shadow = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 offset = vec2(float(x), float(y)) * uShadowTexelSize;
            float closest = texture(uShadowMap, proj.xy + offset).r;
            shadow += (proj.z - bias > closest) ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

// Procedural gradient sky (no HDR environment maps; static lighting comes later).
vec3 fakeEnvironment(vec3 dir) {
    float t = clamp(dir.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 ground = vec3(0.12, 0.11, 0.10);
    vec3 horizon = vec3(0.35, 0.38, 0.42);
    vec3 zenith = vec3(0.55, 0.62, 0.75);
    return mix(ground, mix(horizon, zenith, t), smoothstep(0.0, 1.0, t + 0.15));
}

vec3 sampleEnvironment(vec3 dir, float roughness) {
    return fakeEnvironment(dir);
}

vec3 shadeDirectional(vec3 N, vec3 V, vec3 diffuseColor, vec3 specularColor, vec3 direction,
                      vec3 lightColor, float shadow) {
    vec3 L = normalize(-direction);
    float NdL = max(dot(N, L), 0.0);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), max(uShininess, 1.0));
    // Normalize Blinn lobe a bit so high shininess does not vanish.
    float specNorm = (uShininess + 8.0) / 8.0;
    return (diffuseColor * NdL + specularColor * spec * specNorm) * lightColor * (1.0 - shadow);
}

vec3 shadePoint(vec3 N, vec3 V, vec3 diffuseColor, vec3 specularColor, vec3 position,
                vec3 lightColor, float range) {
    vec3 toLight = position - vWorldPos;
    float dist = length(toLight);
    // Distances in world units (cm).
    float atten = clamp(1.0 - dist / max(range, 0.1), 0.0, 1.0);
    atten *= atten;
    vec3 L = toLight / max(dist, 0.1);
    float NdL = max(dot(N, L), 0.0);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), max(uShininess, 1.0));
    float specNorm = (uShininess + 8.0) / 8.0;
    return (diffuseColor * NdL + specularColor * spec * specNorm) * lightColor * atten;
}

void main() {
    vec2 uv = vTexCoord * uUvScale;
    vec4 texel = texture(uAlbedoMap, uv);
    vec3 albedo = uAlbedo * texel.rgb;
    float alpha = uAlpha * texel.a;
    float metallic = clamp(uMetallic, 0.0, 1.0);
    float roughness = clamp(uRoughness, 0.04, 1.0);

    vec3 tangentNormal = texture(uNormalMap, uv).xyz * 2.0 - 1.0;
    vec3 N = normalize(vTBN * tangentNormal);
    vec3 V = normalize(uCameraPos.xyz - vWorldPos);

    // Metals: little diffuse, specular tinted by albedo. Dielectrics: white-ish F0 from uSpecular.
    vec3 diffuseColor = albedo * (1.0 - metallic);
    vec3 specularColor = mix(uSpecular, albedo, metallic);

    float shadow = 0.0;
    if (uReceiveShadows != 0 && uDirCount > 0) {
        vec3 L0 = normalize(-uDirDirections[0].xyz);
        shadow = shadowFactor(vLightSpacePos, N, L0);
    }

    vec3 lit;
    {
        lit = 0.10 * diffuseColor;
        for (int i = 0; i < MAX_DIR; ++i) {
            if (i >= uDirCount) {
                break;
            }
            float s = (i == 0) ? shadow : 0.0;
            lit += shadeDirectional(N, V, diffuseColor, specularColor, uDirDirections[i].xyz,
                                    uDirColors[i].xyz, s);
        }
        for (int i = 0; i < MAX_POINT; ++i) {
            if (i >= uPointCount) {
                break;
            }
            lit += shadePoint(N, V, diffuseColor, specularColor, uPointPositions[i].xyz,
                              uPointColors[i].xyz, uPointRanges[i].x);
        }
    }

    // Environment reflection — LOD rises with roughness; metals pick up more of the sky.
    // Anti-flicker only when R is nearly constant across the pixel (flat faces locking one texel).
    // Large floors have high fwidth(R) — that must NOT force max LOD or mirrors vanish.
    vec3 R = reflect(-V, N);
    float rDeriv = length(fwidth(R));
    float lockRisk = 1.0 - smoothstep(0.0015, 0.03, rDeriv);
    float envRough = max(roughness, lockRisk * 0.20);
    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 5.0);
    float envStrength = mix(0.08, 0.78, metallic) + fresnel * mix(0.25, 0.5, metallic);
    envStrength *= mix(1.0, 0.4, envRough);
    lit += sampleEnvironment(R, envRough) * specularColor * envStrength;

    // Planar scene mirror (objects + sky), projective sample from the reflection pass.
    if (uHasPlanarReflection != 0) {
        vec4 rc = uReflectionViewProj * vec4(vWorldPos, 1.0);
        // rc.w is the view depth (cm).
        vec2 uv = rc.xy / max(abs(rc.w), 1e-2) * 0.5 + 0.5;
        if (uv.x > 0.0 && uv.x < 1.0 && uv.y > 0.0 && uv.y < 1.0 && rc.w > 0.0) {
            vec3 mirrorColor = texture(uPlanarReflection, uv).rgb;
            float mirrorW = mix(0.35, 0.92, metallic) * mix(1.0, 0.25, roughness);
            mirrorW *= mix(0.7, 1.0, fresnel);
            lit = mix(lit, mirrorColor, clamp(mirrorW, 0.0, 1.0));
        }
    }

    FragColor = vec4(lit, alpha);
}
