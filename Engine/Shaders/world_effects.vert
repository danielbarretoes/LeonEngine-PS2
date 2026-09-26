#version 330 core
// The world's impact marks and tracers (FWorldEffectsRenderer): world-space quads built on the CPU each frame.
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec4 aColor;

uniform mat4 uViewProjection;

out vec2 vTexCoord;
out vec4 vColor;

void main() {
    vTexCoord = aTexCoord;
    vColor = aColor;
    gl_Position = uViewProjection * vec4(aPosition, 1.0);
}
