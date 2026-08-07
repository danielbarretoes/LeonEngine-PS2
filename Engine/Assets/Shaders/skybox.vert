#version 330 core
layout (location = 0) in vec3 aPosition;

out vec3 vDir;

uniform mat4 uView;
uniform mat4 uProjection;

void main() {
    vDir = aPosition;
    mat4 viewNoTranslate = mat4(mat3(uView));
    vec4 clip = uProjection * viewNoTranslate * vec4(aPosition, 1.0);
    gl_Position = clip.xyww;
}
