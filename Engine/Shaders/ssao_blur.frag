#version 330 core

in vec2 vUv;
out float FragAo;

uniform sampler2D uAo;
uniform vec2 uDirection; // texel step (blur axis)

void main() {
    // Spatial Gaussian only. Depth-bilateral blur preserves SSAO depth-quantization bands.
    float result = 0.0;
    const float kernel[9] =
        float[](0.016216, 0.054054, 0.1216216, 0.1945946, 0.227027, 0.1945946, 0.1216216, 0.054054,
                0.016216);
    for (int i = -4; i <= 4; ++i) {
        result += texture(uAo, vUv + uDirection * float(i)).r * kernel[i + 4];
    }
    FragAo = result;
}
