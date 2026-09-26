#version 330 core
// The GS emulator's frame in the window (FGSOpenGLEmulator::Present), at the display's aspect ratio: each window row
// shows one frame line (a whole number of rows per line, nearest), stretched to the width the aspect gives, linearly
// between neighbouring texels as a TV draws the analog line; a 16-bit frame's texels truncated to 5 bits first.
uniform sampler2D uFrame;
uniform ivec2 uOffset;
uniform ivec2 uSize;
uniform int uFrame16;
out vec4 oColor;

vec3 Texel(ivec2 Position)
{
    vec3 Color = texelFetch(uFrame, Position, 0).rgb;
    if (uFrame16 == 1)
    {
        Color = floor(Color * 255.0 / 8.0 + 0.001) * 8.0 / 255.0;
    }
    return Color;
}

void main()
{
    ivec2 FrameSize = textureSize(uFrame, 0);
    vec2 Window = gl_FragCoord.xy - vec2(uOffset);
    int Line = clamp(int(Window.y) * FrameSize.y / uSize.y, 0, FrameSize.y - 1);
    // The texel centres sit at (i + 0.5) of the frame's width across the image.
    float X = clamp(Window.x * float(FrameSize.x) / float(uSize.x) - 0.5, 0.0, float(FrameSize.x - 1));
    int Left = int(floor(X));
    int Right = min(Left + 1, FrameSize.x - 1);
    oColor = vec4(mix(Texel(ivec2(Left, Line)), Texel(ivec2(Right, Line)), X - float(Left)), 1.0);
}
