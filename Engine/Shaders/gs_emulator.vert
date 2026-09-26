#version 330 core
// The GS emulator's vertices (Renderer: FGSOpenGLEmulator): GS window coordinates to OpenGL clip space, the
// attributes interpolated linearly in screen space as the GS does.
layout(location = 0) in vec4 aPosition; // window x, y (GS pixels), depth 0..1, fog 0..255
layout(location = 1) in vec4 aColor;    // 0..255
layout(location = 2) in vec3 aSTQ;
layout(location = 3) in vec2 aUV;       // texels

uniform vec2 uTargetSize;

noperspective out vec4 vColor;
flat out vec4 vFlatColor;
noperspective out vec3 vSTQ;
noperspective out vec2 vUV;
noperspective out float vFog;
noperspective out float vDepth;

const float Bias = 1.0 / 256.0;

void main()
{
    // GS pixel i is sampled at i (+ the bias); OpenGL samples at i + 0.5. The frame's top row is the target's top.
    float X = aPosition.x + 0.5 - Bias;
    float Y = uTargetSize.y - 0.5 - aPosition.y + Bias;
    gl_Position = vec4((X / uTargetSize.x) * 2.0 - 1.0, (Y / uTargetSize.y) * 2.0 - 1.0, 0.0, 1.0);
    vColor = aColor;
    vFlatColor = aColor;
    vSTQ = aSTQ;
    vUV = aUV;
    vFog = aPosition.w;
    vDepth = aPosition.z;
}
