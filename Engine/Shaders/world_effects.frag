#version 330 core
// The world's impact marks and tracers (FWorldEffectsRenderer). uMask is a soft round spot (1 at the centre, 0 at the
// rim). uMode 0: an impact mark, blended by multiplication (GL_DST_COLOR, GL_ZERO): the surface takes vColor.rgb at
// the centre, as strongly as vColor.a, and keeps its colour at the rim. uMode 1: a tracer, added (GL_SRC_ALPHA,
// GL_ONE): vColor.rgb, fading across its width.
in vec2 vTexCoord;
in vec4 vColor;

uniform sampler2D uMask;
uniform int uMode;

out vec4 FragColor;

void main() {
    float mask = texture(uMask, vTexCoord).r;
    if (uMode == 0) {
        FragColor = vec4(mix(vec3(1.0), vColor.rgb, mask * vColor.a), 1.0);
    } else {
        FragColor = vec4(vColor.rgb, mask * vColor.a);
    }
}
