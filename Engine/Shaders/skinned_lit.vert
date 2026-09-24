#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec4 aTangent; // xyz = tangent, w = bitangent handedness
layout (location = 4) in ivec4 aBoneIndices;
layout (location = 5) in vec4 aBoneWeights;

uniform mat4 uModel;
uniform mat4 uMVP;
uniform mat3 uNormalMatrix;
uniform mat4 uLightSpaceMatrix;
uniform vec4 uClipPlane; // ax+by+cz+d; keep when >= 0
uniform int uUseClipPlane;
uniform mat4 uBones[96];

out vec3 vWorldPos;
out vec2 vTexCoord;
out vec4 vLightSpacePos;
out mat3 vTBN;

void main() {
    mat4 skin =
        uBones[aBoneIndices.x] * aBoneWeights.x +
        uBones[aBoneIndices.y] * aBoneWeights.y +
        uBones[aBoneIndices.z] * aBoneWeights.z +
        uBones[aBoneIndices.w] * aBoneWeights.w;

    vec4 skinnedPos = skin * vec4(aPosition, 1.0);
    vec3 skinnedN = mat3(skin) * aNormal;
    vec3 skinnedT = mat3(skin) * aTangent.xyz;

    vec4 worldPos = uModel * skinnedPos;
    vWorldPos = worldPos.xyz;
    vTexCoord = aTexCoord;
    vLightSpacePos = uLightSpaceMatrix * worldPos;

    vec3 N = normalize(uNormalMatrix * skinnedN);
    vec3 T = normalize(uNormalMatrix * skinnedT);
    T = normalize(T - N * dot(N, T));
    vec3 B = cross(N, T) * aTangent.w;
    vTBN = mat3(T, B, N);

    gl_ClipDistance[0] = (uUseClipPlane != 0) ? dot(uClipPlane, worldPos) : 1.0;
    gl_Position = uMVP * skinnedPos;
}
