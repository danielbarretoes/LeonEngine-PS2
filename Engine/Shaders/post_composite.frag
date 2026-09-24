#version 330 core

in vec2 vUv;
out vec4 FragColor;

uniform sampler2D uSceneColor;
uniform sampler2D uAo;
uniform int uUseAo;
uniform float uAoIntensity;
uniform float uAoPower;
uniform float uExposure;

// ACES fitted approximation (Narkowicz).
vec3 acesTonemap(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 hdr = texture(uSceneColor, vUv).rgb * max(uExposure, 0.01);
    float ao = 1.0;
    if (uUseAo != 0) {
        // Bilinear upsample — bilateral upsample kept depth stair-steps as dark lines.
        ao = texture(uAo, vUv).r;
        ao = pow(clamp(ao, 0.0, 1.0), max(uAoPower, 0.01));
        ao = mix(1.0, ao, clamp(uAoIntensity, 0.0, 2.0));
    }
    hdr *= ao;
    FragColor = vec4(acesTonemap(hdr), 1.0);
}
