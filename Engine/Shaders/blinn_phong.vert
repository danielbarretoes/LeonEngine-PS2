#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec4 aTangent; // xyz = tangent, w = bitangent handedness

uniform mat4 uModel;
uniform mat4 uMVP;
uniform mat3 uNormalMatrix;
uniform mat4 uLightSpaceMatrix;
uniform vec4 uClipPlane; // ax+by+cz+d; keep when >= 0
uniform int uUseClipPlane;

out vec3 vWorldPos;
out vec2 vTexCoord;
out vec4 vLightSpacePos;
out mat3 vTBN;

void main() {
    vec4 worldPos = uModel * vec4(aPosition, 1.0);
    vWorldPos = worldPos.xyz;
    vTexCoord = aTexCoord;
    vLightSpacePos = uLightSpaceMatrix * worldPos;

    vec3 N = normalize(uNormalMatrix * aNormal);
    vec3 T = normalize(uNormalMatrix * aTangent.xyz);
    T = normalize(T - N * dot(N, T));
    vec3 B = cross(N, T) * aTangent.w;
    vTBN = mat3(T, B, N);

    gl_ClipDistance[0] = (uUseClipPlane != 0) ? dot(uClipPlane, worldPos) : 1.0;
    gl_Position = uMVP * vec4(aPosition, 1.0);
}
