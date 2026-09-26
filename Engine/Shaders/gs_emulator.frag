#version 330 core
// The GS emulator's pixel pipeline (Renderer: FGSOpenGLEmulator), after the reference rasterizer
// (Developer/GSReference): texture sampling and function, fog, the alpha test, dithering, and the blend factor for
// dual-source blending.
noperspective in vec4 vColor;
flat in vec4 vFlatColor;
noperspective in vec3 vSTQ;
noperspective in vec2 vUV;
noperspective in float vFog;
noperspective in float vDepth;

layout(location = 0, index = 0) out vec4 oColor;
layout(location = 0, index = 1) out vec4 oFactor;

uniform int uGouraud;
uniform int uTextured;
uniform int uUseUV;
uniform int uTFX;
uniform int uTCC;
uniform int uFog;
uniform ivec3 uFogColor;
uniform sampler2D uTexture;
uniform ivec2 uBaseSize;   // 2^TW, 2^TH
uniform ivec2 uLevelSize;
uniform int uLevel;
uniform int uBilinear;
uniform ivec2 uWrap;
uniform ivec4 uRegion;     // MINU, MAXU, MINV, MAXV
uniform int uAlphaTest;
uniform int uAlphaTestMode;
uniform int uAlphaRef;
uniform int uAlphaPass;    // 0: what passes the alpha test, 1: what fails it
uniform int uBlend;
uniform int uBlendFixed;
uniform int uFix;
uniform int uPABE;
uniform int uFrame16;
uniform int uDither;
uniform int uDimx[16];
uniform int uFBA;
uniform int uTargetHeight;

int RoundByte(float Value)
{
    return clamp(int(floor(Value + 0.5)), 0, 255);
}

int ClampByte(int Value)
{
    return clamp(Value, 0, 255);
}

int Wrap(int Mode, int Coordinate, int Size, int Min, int Max)
{
    if (Mode == 0) { return Coordinate & (Size - 1); }
    if (Mode == 1) { return clamp(Coordinate, 0, Size - 1); }
    if (Mode == 2) { return clamp(Coordinate, Min >> uLevel, Max >> uLevel); }
    return (Coordinate & Min) | Max;
}

ivec4 Fetch(int U, int V)
{
    int X = Wrap(uWrap.x, U, uLevelSize.x, uRegion.x, uRegion.y);
    int Y = Wrap(uWrap.y, V, uLevelSize.y, uRegion.z, uRegion.w);
    return ivec4(floor(texelFetch(uTexture, ivec2(X & 2047, Y & 2047), 0) * 255.0 + 0.5));
}

ivec4 SampleTexture()
{
    float U;
    float V;
    if (uUseUV == 1)
    {
        U = vUV.x;
        V = vUV.y;
    }
    else
    {
        float Q = vSTQ.z != 0.0 ? vSTQ.z : 1.0e-30;
        U = (vSTQ.x / Q) * float(uBaseSize.x);
        V = (vSTQ.y / Q) * float(uBaseSize.y);
    }
    U = clamp(U, -2047.0, 2047.0) / float(1 << uLevel);
    V = clamp(V, -2047.0, 2047.0) / float(1 << uLevel);
    if (uBilinear == 0)
    {
        return Fetch(int(floor(U)), int(floor(V)));
    }
    float BaseU = U - 0.5;
    float BaseV = V - 0.5;
    int U0 = int(floor(BaseU));
    int V0 = int(floor(BaseV));
    float Alpha = BaseU - float(U0);
    float Beta = BaseV - float(V0);
    vec4 Sum = (1.0 - Alpha) * (1.0 - Beta) * vec4(Fetch(U0, V0)) + Alpha * (1.0 - Beta) * vec4(Fetch(U0 + 1, V0)) +
        (1.0 - Alpha) * Beta * vec4(Fetch(U0, V0 + 1)) + Alpha * Beta * vec4(Fetch(U0 + 1, V0 + 1));
    return ivec4(clamp(floor(Sum + 0.5), 0.0, 255.0));
}

bool AlphaTestPasses(int Alpha)
{
    if (uAlphaTestMode == 0) { return false; }
    if (uAlphaTestMode == 1) { return true; }
    if (uAlphaTestMode == 2) { return Alpha < uAlphaRef; }
    if (uAlphaTestMode == 3) { return Alpha <= uAlphaRef; }
    if (uAlphaTestMode == 4) { return Alpha == uAlphaRef; }
    if (uAlphaTestMode == 5) { return Alpha >= uAlphaRef; }
    if (uAlphaTestMode == 6) { return Alpha > uAlphaRef; }
    return Alpha != uAlphaRef;
}

void main()
{
    vec4 Vertex = uGouraud == 1 ? vColor : vFlatColor;
    int R = RoundByte(Vertex.r);
    int G = RoundByte(Vertex.g);
    int B = RoundByte(Vertex.b);
    int A = RoundByte(Vertex.a);

    if (uTextured == 1)
    {
        // The texture function (manual 3.4.9): A * B = (A x B) >> 7, clamped.
        ivec4 Texel = SampleTexture();
        if (uTFX == 0)
        {
            R = ClampByte((Texel.r * R) >> 7);
            G = ClampByte((Texel.g * G) >> 7);
            B = ClampByte((Texel.b * B) >> 7);
            A = uTCC == 1 ? ClampByte((Texel.a * A) >> 7) : A;
        }
        else if (uTFX == 1)
        {
            R = Texel.r;
            G = Texel.g;
            B = Texel.b;
            A = uTCC == 1 ? Texel.a : A;
        }
        else
        {
            int FragmentAlpha = A;
            R = ClampByte(((Texel.r * R) >> 7) + FragmentAlpha);
            G = ClampByte(((Texel.g * G) >> 7) + FragmentAlpha);
            B = ClampByte(((Texel.b * B) >> 7) + FragmentAlpha);
            if (uTCC == 1)
            {
                A = uTFX == 2 ? ClampByte(Texel.a + FragmentAlpha) : Texel.a;
            }
        }
    }

    if (uFog == 1)
    {
        // C = F * C + (0xff - F) * FOGCOL, >> 8 (manual 3.5).
        int F = RoundByte(vFog);
        R = ((F * R) + ((255 - F) * uFogColor.r)) >> 8;
        G = ((F * G) + ((255 - F) * uFogColor.g)) >> 8;
        B = ((F * B) + ((255 - F) * uFogColor.b)) >> 8;
    }

    if (uAlphaTest == 1 && AlphaTestPasses(A) == (uAlphaPass == 1))
    {
        discard;
    }

    bool bBlended = uBlend == 1 && (uPABE == 0 || (A & 0x80) != 0);
    if (uFrame16 == 1 && !bBlended)
    {
        // Dithered (DIMX[Y % 4][X % 4]), clamped, then 5 bits a channel (manual 3.9.1).
        // The GS pixel: OpenGL's row 0 is the frame's bottom row.
        int PixelX = int(gl_FragCoord.x);
        int PixelY = uTargetHeight - 1 - int(gl_FragCoord.y);
        int Offset = uDither == 1 ? uDimx[((PixelY & 3) << 2) | (PixelX & 3)] : 0;
        R = (ClampByte(R + Offset) >> 3) << 3;
        G = (ClampByte(G + Offset) >> 3) << 3;
        B = (ClampByte(B + Offset) >> 3) << 3;
    }

    int Factor = uBlendFixed == 1 ? uFix : A;
    int StoredAlpha = uFBA == 1 ? (A | 0x80) : A;
    oColor = vec4(float(R) / 255.0, float(G) / 255.0, float(B) / 255.0, float(StoredAlpha) / 255.0);
    oFactor = vec4(0.0, 0.0, 0.0, bBlended ? float(Factor) / 128.0 : 1.0);
    gl_FragDepth = vDepth;
}
