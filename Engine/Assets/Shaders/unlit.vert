#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 uModel;
uniform mat4 uMVP;
uniform vec4 uClipPlane;
uniform int uUseClipPlane;

out vec2 vTexCoord;

void main() {
    vec4 worldPos = uModel * vec4(aPosition, 1.0);
    vTexCoord = aTexCoord;
    gl_ClipDistance[0] = (uUseClipPlane != 0) ? dot(uClipPlane, worldPos) : 1.0;
    gl_Position = uMVP * vec4(aPosition, 1.0);
}
