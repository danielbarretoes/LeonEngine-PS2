#version 330 core
// The GS emulator's pixel pipeline (Renderer: FGSOpenGLEmulator), after the reference rasterizer
// (Developer/GSReference): the LOD and MIPMAP levels, texture sampling and function, fog, the alpha and destination
// alpha tests, blending with the whole (A - B) * C >> 7 + D, dithering, the colour clamp, FBA, the 16-bit packing and
// FBMSK. A draw that reads the frame buffer (uReadsFrame) gets its destination from a copy of the frame the emulator
// makes before each group of primitives that cover no pixel twice.
noperspective in vec4 vColor;
flat in vec4 vFlatColor;
noperspective in vec3 vSTQ;
noperspective in vec2 vUV;
noperspective in float vFog;
noperspective in float vDepth;

layout(location = 0) out vec4 oColor;

uniform int uGouraud;
uniform int uTextured;
uniform int uUseUV;
uniform int uTFX;
uniform int uTCC;
uniform int uFog;
uniform ivec3 uFogColor;
uniform sampler2DArray uTexture; // a layer per MIPMAP level
uniform ivec2 uBaseSize;         // 2^TW, 2^TH
uniform ivec2 uLevelSize[7];
uniform int uMaxLevel;           // the last level decoded (MXL for the MIPMAP filters, 0 otherwise)
uniform int uFixedLOD;           // LCM
uniform int uL;
uniform float uK;
uniform int uMMAG;
uniform int uMMIN;
uniform ivec2 uWrap;
uniform ivec4 uRegion;           // MINU, MAXU, MINV, MAXV
uniform int uAlphaTest;
uniform int uAlphaTestMode;
uniform int uAlphaRef;
uniform int uAlphaPass;          // 0: what passes the alpha test, 1: what fails it
uniform int uReadsFrame;
uniform sampler2D uDestination;
uniform int uDestAlphaTest;
uniform int uDestAlphaOne;
uniform int uBlend;
uniform ivec4 uBlendInputs;      // ALPHA's A, B, C, D
uniform int uFix;
uniform int uPABE;
uniform int uFrameFormat;        // 0: PSMCT32, 1: PSMCT24, 2: PSMCT16 / PSMCT16S
uniform int uDither;
uniform int uDimx[16];
uniform int uColorClamp;
uniform int uFBA;
uniform int uKeepMask;           // the frame's bits a pixel keeps (FBMSK, AFAIL), when it reads the frame
uniform int uTargetHeight;

int RoundByte(float Value)
{
    return clamp(int(floor(Value + 0.5)), 0, 255);
}

int ClampByte(int Value)
{
    return clamp(Value, 0, 255);
}

// A texel coordinate of level Level by the wrap mode (manual 3.4.5); the region fields shift with the level.
int Wrap(int Mode, int Coordinate, int Size, int Min, int Max, int Level)
{
    if (Mode == 0) { return Coordinate & (Size - 1); }
    if (Mode == 1) { return clamp(Coordinate, 0, Size - 1); }
    if (Mode == 2)
    {
        int Low = Min >> Level;
        int High = Max >> Level;
        return Coordinate < Low ? Low : (Coordinate < High ? Coordinate : High);
    }
    return (Coordinate & (Min >> Level)) | (Max >> Level);
}

ivec4 Fetch(int Level, int U, int V)
{
    ivec2 Size = uLevelSize[Level];
    int X = Wrap(uWrap.x, U, Size.x, uRegion.x, uRegion.y, Level);
    int Y = Wrap(uWrap.y, V, Size.y, uRegion.z, uRegion.w, Level);
    return ivec4(floor(texelFetch(uTexture, ivec3(X, Y, Level), 0) * 255.0 + 0.5));
}

// One level at level-0 texel coordinates (U, V): the texel under the point, or the four around it with their
// centers at .5 (manual 3.4.8).
ivec4 FilterLevel(int Level, float U, float V, bool bBilinear)
{
    float Scale = 1.0 / float(1 << Level);
    float LevelU = U * Scale;
    float LevelV = V * Scale;
    if (!bBilinear)
    {
        return Fetch(Level, int(floor(LevelU)), int(floor(LevelV)));
    }
    float BaseU = LevelU - 0.5;
    float BaseV = LevelV - 0.5;
    int U0 = int(floor(BaseU));
    int V0 = int(floor(BaseV));
    float Alpha = BaseU - float(U0);
    float Beta = BaseV - float(V0);
    vec4 Sum = (1.0 - Alpha) * (1.0 - Beta) * vec4(Fetch(Level, U0, V0)) +
        Alpha * (1.0 - Beta) * vec4(Fetch(Level, U0 + 1, V0)) + (1.0 - Alpha) * Beta * vec4(Fetch(Level, U0, V0 + 1)) +
        Alpha * Beta * vec4(Fetch(Level, U0 + 1, V0 + 1));
    return ivec4(clamp(floor(Sum + 0.5), 0.0, 255.0));
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
        // s = S / Q, t = T / Q (manual 3.4.10).
        float Q = vSTQ.z != 0.0 ? vSTQ.z : 1.0e-30;
        U = (vSTQ.x / Q) * float(uBaseSize.x);
        V = (vSTQ.y / Q) * float(uBaseSize.y);
    }
    U = clamp(U, -2047.0, 2047.0);
    V = clamp(V, -2047.0, 2047.0);

    // LOD = (log2(1 / |Q|) << L) + K, or K (manual 3.4.12); Q is the fragment's, with UV too.
    float Lod;
    if (uFixedLOD == 1)
    {
        Lod = uK;
    }
    else
    {
        float AbsQ = abs(vSTQ.z);
        Lod = AbsQ > 0.0 ? log2(1.0 / AbsQ) * float(1 << uL) + uK : 64.0;
    }
    if (Lod <= 0.0)
    {
        return FilterLevel(0, U, V, uMMAG == 1);
    }
    if (uMMIN <= 1)
    {
        return FilterLevel(0, U, V, uMMIN == 1);
    }
    bool bBilinear = uMMIN == 4 || uMMIN == 5;
    if (uMMIN == 2 || uMMIN == 4)
    {
        // Level m or m + 1 by the LOD, rounded.
        return FilterLevel(min(int(floor(Lod + 0.5)), uMaxLevel), U, V, bBilinear);
    }
    // Levels m and m + 1 blended by the LOD's fraction.
    int Level = min(int(floor(Lod)), uMaxLevel);
    if (Level >= uMaxLevel)
    {
        return FilterLevel(uMaxLevel, U, V, bBilinear);
    }
    float Weight = Lod - float(Level);
    vec4 Near = vec4(FilterLevel(Level, U, V, bBilinear));
    vec4 Far = vec4(FilterLevel(Level + 1, U, V, bBilinear));
    return ivec4(clamp(floor(Near + (Far - Near) * Weight + 0.5), 0.0, 255.0));
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

ivec3 BlendColor(int Input, ivec3 Source, ivec3 Destination)
{
    return Input == 0 ? Source : (Input == 1 ? Destination : ivec3(0));
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

    // The destination (a 16-bit frame holds its channels shifted left 3 and its alpha bit as 0x80).
    ivec4 Destination = ivec4(0);
    if (uReadsFrame == 1)
    {
        Destination = ivec4(floor(texelFetch(uDestination, ivec2(gl_FragCoord.xy), 0) * 255.0 + 0.5));
        // The destination alpha test: bit 7 of A, the 16-bit alpha bit; a 24-bit frame always passes (manual 3.7.3).
        if (uDestAlphaTest == 1 && uFrameFormat != 1 && ((Destination.a & 0x80) != 0) != (uDestAlphaOne == 1))
        {
            discard;
        }
    }

    ivec3 Color = ivec3(R, G, B);
    if (uBlend == 1 && (uPABE == 0 || (A & 0x80) != 0))
    {
        // (A - B) * C >> 7 + D, not clamped until after dithering; Ad is 0x80 for a 24-bit frame (manual 3.8).
        int DestinationAlpha = uFrameFormat == 1 ? 0x80 : Destination.a;
        int Factor = uBlendInputs.z == 0 ? A : (uBlendInputs.z == 1 ? DestinationAlpha : uFix);
        ivec3 Difference = (BlendColor(uBlendInputs.x, Color, Destination.rgb) -
            BlendColor(uBlendInputs.y, Color, Destination.rgb)) * Factor;
        // >> 7 toward minus infinity (exact: a power of two).
        Color = ivec3(floor(vec3(Difference) / 128.0)) + BlendColor(uBlendInputs.w, Color, Destination.rgb);
    }

    // Dithering (a 16-bit frame: DIMX[Y % 4][X % 4], manual 3.9.1), then the colour clamp or the low 8 bits (3.9.2).
    if (uFrameFormat == 2 && uDither == 1)
    {
        // The GS pixel: OpenGL's row 0 is the frame's bottom row.
        int PixelX = int(gl_FragCoord.x);
        int PixelY = uTargetHeight - 1 - int(gl_FragCoord.y);
        Color += ivec3(uDimx[((PixelY & 3) << 2) | (PixelX & 3)]);
    }
    Color = uColorClamp == 1 ? clamp(Color, 0, 255) : (Color & 255);
    // FBA sets the written alpha's MSB (3.9.3); a 16-bit frame keeps 5 bits a channel and the alpha bit (3.9.4).
    int StoredAlpha = uFBA == 1 ? (A | 0x80) : A;
    if (uFrameFormat == 2)
    {
        Color &= 0xf8;
        StoredAlpha &= 0x80;
    }

    if (uReadsFrame == 1)
    {
        // FBMSK and what AFAIL holds back, bit by bit (3.9.5).
        uint Keep = uint(uKeepMask);
        uint Value = uint(Color.r) | (uint(Color.g) << 8) | (uint(Color.b) << 16) | (uint(StoredAlpha) << 24);
        uint Previous = uint(Destination.r) | (uint(Destination.g) << 8) | (uint(Destination.b) << 16) |
            (uint(Destination.a) << 24);
        uint Written = (Previous & Keep) | (Value & ~Keep);
        Color = ivec3(int(Written & 0xffu), int((Written >> 8) & 0xffu), int((Written >> 16) & 0xffu));
        StoredAlpha = int(Written >> 24);
    }
    oColor = vec4(vec3(Color) / 255.0, float(StoredAlpha) / 255.0);
    gl_FragDepth = vDepth;
}
