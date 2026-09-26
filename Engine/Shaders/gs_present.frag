#version 330 core
// The GS emulator's frame in the window, scaled by a whole number, nearest; a 16-bit frame truncated to 5 bits.
uniform sampler2D uFrame;
uniform ivec2 uOffset;
uniform int uScale;
uniform int uFrame16;
out vec4 oColor;

void main()
{
    ivec2 Texel = (ivec2(gl_FragCoord.xy) - uOffset) / uScale;
    vec3 Color = texelFetch(uFrame, Texel, 0).rgb;
    if (uFrame16 == 1)
    {
        Color = floor(Color * 255.0 / 8.0 + 0.001) * 8.0 / 255.0;
    }
    oColor = vec4(Color, 1.0);
}
