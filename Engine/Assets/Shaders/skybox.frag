#version 330 core
in vec3 vDir;

uniform samplerCube uEnvMap;
uniform float uEnvExposure;

out vec4 FragColor;

void main() {
    vec3 color = texture(uEnvMap, normalize(vDir)).rgb;
    // Slightly hotter than material env samples so the backdrop reads clearly.
    float exposure = uEnvExposure * 1.45;
    color = vec3(1.0) - exp(-color * exposure);
    FragColor = vec4(color, 1.0);
}
