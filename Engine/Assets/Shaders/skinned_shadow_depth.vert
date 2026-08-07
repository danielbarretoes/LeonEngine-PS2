#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 4) in ivec4 aBoneIndices;
layout (location = 5) in vec4 aBoneWeights;

uniform mat4 uLightMVP;
uniform mat4 uBones[96];

void main() {
    mat4 skin =
        uBones[aBoneIndices.x] * aBoneWeights.x +
        uBones[aBoneIndices.y] * aBoneWeights.y +
        uBones[aBoneIndices.z] * aBoneWeights.z +
        uBones[aBoneIndices.w] * aBoneWeights.w;

    vec4 skinnedPos = skin * vec4(aPosition, 1.0);
    gl_Position = uLightMVP * skinnedPos;
}
