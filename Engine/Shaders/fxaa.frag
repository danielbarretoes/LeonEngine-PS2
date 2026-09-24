#version 330 core

in vec2 vUv;
out vec4 FragColor;

uniform sampler2D uColor;
uniform vec2 uInvResolution;

// FXAA 3.11 (trimmed) — Jimenez et al. / NVIDIA.
#define FXAA_REDUCE_MIN (1.0 / 128.0)
#define FXAA_REDUCE_MUL (1.0 / 8.0)
#define FXAA_SPAN_MAX 8.0

void main() {
    vec2 rcp = uInvResolution;
    vec3 rgbNW = texture(uColor, vUv + vec2(-1.0, -1.0) * rcp).rgb;
    vec3 rgbNE = texture(uColor, vUv + vec2(1.0, -1.0) * rcp).rgb;
    vec3 rgbSW = texture(uColor, vUv + vec2(-1.0, 1.0) * rcp).rgb;
    vec3 rgbSE = texture(uColor, vUv + vec2(1.0, 1.0) * rcp).rgb;
    vec3 rgbM = texture(uColor, vUv).rgb;

    const vec3 luma = vec3(0.299, 0.587, 0.114);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM = dot(rgbM, luma);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y = ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL),
                          FXAA_REDUCE_MIN);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, vec2(-FXAA_SPAN_MAX), vec2(FXAA_SPAN_MAX)) * rcp;

    vec3 rgbA = 0.5 * (texture(uColor, vUv + dir * (1.0 / 3.0 - 0.5)).rgb +
                       texture(uColor, vUv + dir * (2.0 / 3.0 - 0.5)).rgb);
    vec3 rgbB = rgbA * 0.5 + 0.25 * (texture(uColor, vUv + dir * -0.5).rgb +
                                     texture(uColor, vUv + dir * 0.5).rgb);

    float lumaB = dot(rgbB, luma);
    if (lumaB < lumaMin || lumaB > lumaMax) {
        FragColor = vec4(rgbA, 1.0);
    } else {
        FragColor = vec4(rgbB, 1.0);
    }
}
