#version 330 core
in vec2 vTexCoord;

uniform vec3 uAlbedo;
uniform float uAlpha;
uniform vec2 uUvScale;
uniform sampler2D uAlbedoMap;

out vec4 FragColor;

void main() {
    vec4 texel = texture(uAlbedoMap, vTexCoord * uUvScale);
    // Ignore map alpha so incomplete/black-border samples cannot ghost through the swapchain.
    FragColor = vec4(uAlbedo * texel.rgb, uAlpha);
}
