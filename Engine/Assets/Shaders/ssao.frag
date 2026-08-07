#version 330 core

in vec2 vUv;
out float FragAo;

uniform sampler2D uDepth;
uniform sampler2D uNoise;
uniform vec3 uSamples[64];
uniform int uSampleCount;
uniform mat4 uProjection;
uniform mat4 uInvProjection;
uniform vec2 uNoiseScale;
uniform float uRadius;
uniform float uBias;

vec3 viewPosFromDepth(vec2 uv, float depth) {
    vec4 clip = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 view = uInvProjection * clip;
    return view.xyz / view.w;
}

// Wide taps average out 24-bit depth quantization (1px taps create striped normals).
vec3 reconstructNormal(vec2 uv, float depth) {
    vec3 p = viewPosFromDepth(uv, depth);
    vec2 texel = 1.0 / vec2(textureSize(uDepth, 0));
    const float k = 3.0;

    vec3 pR = viewPosFromDepth(uv + vec2(texel.x * k, 0.0),
                               texture(uDepth, uv + vec2(texel.x * k, 0.0)).r);
    vec3 pL = viewPosFromDepth(uv - vec2(texel.x * k, 0.0),
                               texture(uDepth, uv - vec2(texel.x * k, 0.0)).r);
    vec3 pU = viewPosFromDepth(uv + vec2(0.0, texel.y * k),
                               texture(uDepth, uv + vec2(0.0, texel.y * k)).r);
    vec3 pD = viewPosFromDepth(uv - vec2(0.0, texel.y * k),
                               texture(uDepth, uv - vec2(0.0, texel.y * k)).r);

    vec3 dX = (abs(pR.z - p.z) < abs(p.z - pL.z)) ? (pR - p) : (p - pL);
    vec3 dY = (abs(pU.z - p.z) < abs(p.z - pD.z)) ? (pU - p) : (p - pD);
    vec3 n = cross(dX, dY);
    float len2 = dot(n, n);
    if (len2 < 1.0e-10) {
        return vec3(0.0, 0.0, 1.0);
    }
    n = n * inversesqrt(len2);
    return (n.z < 0.0) ? -n : n;
}

void main() {
    float depth = texture(uDepth, vUv).r;
    if (depth >= 0.9999) {
        FragAo = 1.0;
        return;
    }

    vec3 fragPos = viewPosFromDepth(vUv, depth);
    // Depth-scaled bias hides perspective depth-buffer stair steps.
    float z = max(-fragPos.z, 1.0e-3);
    float bias = max(uBias, z * 0.02);

    vec3 normal = reconstructNormal(vUv, depth);
    vec3 randomVec = normalize(texture(uNoise, vUv * uNoiseScale).xyz * 2.0 - 1.0);

    vec3 tangent = randomVec - normal * dot(randomVec, normal);
    float tLen2 = dot(tangent, tangent);
    if (tLen2 < 1.0e-6) {
        tangent = abs(normal.y) < 0.999 ? cross(normal, vec3(0.0, 1.0, 0.0))
                                        : cross(normal, vec3(1.0, 0.0, 0.0));
    } else {
        tangent *= inversesqrt(tLen2);
    }
    vec3 bitangent = cross(normal, tangent);
    mat3 tbn = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    float contrib = 0.0;
    int count = clamp(uSampleCount, 1, 64);
    for (int i = 0; i < count; ++i) {
        vec3 samplePos = fragPos + tbn * uSamples[i] * uRadius;
        vec4 offset = uProjection * vec4(samplePos, 1.0);
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5 + 0.5;

        if (offset.x < 0.0 || offset.x > 1.0 || offset.y < 0.0 || offset.y > 1.0) {
            continue;
        }

        float sampleDepth = viewPosFromDepth(offset.xy, texture(uDepth, offset.xy).r).z;
        float dz = sampleDepth - samplePos.z;
        // Soft compare: hard step against quantized depth produces parallel bands.
        float occluded = smoothstep(bias, bias + uRadius * 0.35, dz);
        float rangeCheck = 1.0 - smoothstep(0.0, uRadius, abs(fragPos.z - sampleDepth));
        occlusion += occluded * rangeCheck;
        contrib += 1.0;
    }

    FragAo = 1.0 - (occlusion / max(contrib, 1.0));
}
