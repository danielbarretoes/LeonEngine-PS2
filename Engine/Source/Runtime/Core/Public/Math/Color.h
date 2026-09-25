#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Math/MathFwd.h"
#include "Math/UnrealMathUtility.h"
#include "Serialization/Archive.h"
#include "Templates/TypeHash.h"

/** Linear-space RGBA color in floats (UE: FLinearColor). */
struct CORE_API FLinearColor
{
	float R;
	float G;
	float B;
	float A;

	/** sRGB byte to linear float (UE: sRGBToLinearTable). */
	static const float sRGBToLinearTable[256]; // NOLINT(readability-identifier-naming): UE name

	static const FLinearColor White;
	static const FLinearColor Gray;
	static const FLinearColor Black;
	static const FLinearColor Transparent;
	static const FLinearColor Red;
	static const FLinearColor Green;
	static const FLinearColor Blue;
	static const FLinearColor Yellow;

	/** Uninitialised (UE). */
	FLinearColor() = default;

	explicit FORCEINLINE FLinearColor(EForceInit)
		: R(0)
		, G(0)
		, B(0)
		, A(0)
	{
	}

	constexpr FORCEINLINE FLinearColor(float InR, float InG, float InB, float InA = 1.0f)
		: R(InR)
		, G(InG)
		, B(InB)
		, A(InA)
	{
	}

	/** From an sRGB FColor (UE: FLinearColor(const FColor&)). */
	FLinearColor(const FColor& Color);

	explicit FLinearColor(const FVector& Vector);
	explicit FLinearColor(const FVector4& Vector);

	/** Same as the FColor constructor (UE: FromSRGBColor). */
	static FLinearColor FromSRGBColor(const FColor& Color);

	/** From a gamma 2.2 FColor (UE: FromPow22Color). */
	static FLinearColor FromPow22Color(const FColor& Color);

	FORCEINLINE float& Component(int32 Index)
	{
		return (&R)[Index];
	}

	FORCEINLINE const float& Component(int32 Index) const
	{
		return (&R)[Index];
	}

	FORCEINLINE FLinearColor operator+(const FLinearColor& ColorB) const
	{
		return FLinearColor(R + ColorB.R, G + ColorB.G, B + ColorB.B, A + ColorB.A);
	}
	FORCEINLINE FLinearColor& operator+=(const FLinearColor& ColorB)
	{
		R += ColorB.R;
		G += ColorB.G;
		B += ColorB.B;
		A += ColorB.A;
		return *this;
	}

	FORCEINLINE FLinearColor operator-(const FLinearColor& ColorB) const
	{
		return FLinearColor(R - ColorB.R, G - ColorB.G, B - ColorB.B, A - ColorB.A);
	}
	FORCEINLINE FLinearColor& operator-=(const FLinearColor& ColorB)
	{
		R -= ColorB.R;
		G -= ColorB.G;
		B -= ColorB.B;
		A -= ColorB.A;
		return *this;
	}

	FORCEINLINE FLinearColor operator*(const FLinearColor& ColorB) const
	{
		return FLinearColor(R * ColorB.R, G * ColorB.G, B * ColorB.B, A * ColorB.A);
	}
	FORCEINLINE FLinearColor& operator*=(const FLinearColor& ColorB)
	{
		R *= ColorB.R;
		G *= ColorB.G;
		B *= ColorB.B;
		A *= ColorB.A;
		return *this;
	}

	FORCEINLINE FLinearColor operator*(float Scalar) const
	{
		return FLinearColor(R * Scalar, G * Scalar, B * Scalar, A * Scalar);
	}
	FORCEINLINE FLinearColor& operator*=(float Scalar)
	{
		R *= Scalar;
		G *= Scalar;
		B *= Scalar;
		A *= Scalar;
		return *this;
	}

	FORCEINLINE FLinearColor operator/(const FLinearColor& ColorB) const
	{
		return FLinearColor(R / ColorB.R, G / ColorB.G, B / ColorB.B, A / ColorB.A);
	}
	FORCEINLINE FLinearColor& operator/=(const FLinearColor& ColorB)
	{
		R /= ColorB.R;
		G /= ColorB.G;
		B /= ColorB.B;
		A /= ColorB.A;
		return *this;
	}

	FORCEINLINE FLinearColor operator/(float Scalar) const
	{
		const float InvScalar = 1.0f / Scalar;
		return FLinearColor(R * InvScalar, G * InvScalar, B * InvScalar, A * InvScalar);
	}
	FORCEINLINE FLinearColor& operator/=(float Scalar)
	{
		const float InvScalar = 1.0f / Scalar;
		R *= InvScalar;
		G *= InvScalar;
		B *= InvScalar;
		A *= InvScalar;
		return *this;
	}

	/** Each component clamped to [InMin, InMax] (UE: GetClamped). */
	FORCEINLINE FLinearColor GetClamped(float InMin = 0.0f, float InMax = 1.0f) const
	{
		FLinearColor Ret;
		Ret.R = FMath::Clamp(R, InMin, InMax);
		Ret.G = FMath::Clamp(G, InMin, InMax);
		Ret.B = FMath::Clamp(B, InMin, InMax);
		Ret.A = FMath::Clamp(A, InMin, InMax);
		return Ret;
	}

	FORCEINLINE bool operator==(const FLinearColor& ColorB) const
	{
		return R == ColorB.R && G == ColorB.G && B == ColorB.B && A == ColorB.A;
	}
	FORCEINLINE bool operator!=(const FLinearColor& Other) const
	{
		return R != Other.R || G != Other.G || B != Other.B || A != Other.A;
	}

	FORCEINLINE bool Equals(const FLinearColor& ColorB, float Tolerance = KINDA_SMALL_NUMBER) const
	{
		return FMath::Abs(R - ColorB.R) < Tolerance && FMath::Abs(G - ColorB.G) < Tolerance &&
			FMath::Abs(B - ColorB.B) < Tolerance && FMath::Abs(A - ColorB.A) < Tolerance;
	}

	FORCEINLINE FLinearColor CopyWithNewOpacity(float NewOpacity) const
	{
		FLinearColor NewCopy = *this;
		NewCopy.A = NewOpacity;
		return NewCopy;
	}

	/** Hue in degrees, saturation and value to linear RGB; this color holds H, S, V in R, G, B (UE: HSVToLinearRGB). */
	FLinearColor HSVToLinearRGB() const;

	/** Linear RGB to H (degrees), S, V stored in R, G, B (UE: LinearRGBToHSV). */
	FLinearColor LinearRGBToHSV() const;

	/** Linear color from 8-bit hue, saturation and value (UE: MakeFromHSV8). */
	static FLinearColor MakeFromHSV8(uint8 H, uint8 S, uint8 V);

	/** Random saturated color (UE: MakeRandomColor). */
	static FLinearColor MakeRandomColor();

	/** Interpolates in HSV along the shortest hue path (UE: LerpUsingHSV). */
	static FLinearColor LerpUsingHSV(const FLinearColor& From, const FLinearColor& To, float Progress);

	/** Byte color, with the sRGB curve when bSRGB (UE: ToFColor). */
	FColor ToFColor(bool bSRGB) const;

	/** Byte color by truncation / rounding, no curve (UE: Quantize / QuantizeRound). */
	FColor Quantize() const;
	FColor QuantizeRound() const;

	/** Mix toward grey by Desaturation (UE: Desaturate). */
	FLinearColor Desaturate(float Desaturation) const;

	/** Perceived brightness (UE: ComputeLuminance / GetLuminance). */
	FORCEINLINE float ComputeLuminance() const
	{
		return R * 0.3f + G * 0.59f + B * 0.11f;
	}
	FORCEINLINE float GetLuminance() const
	{
		return R * 0.3f + G * 0.59f + B * 0.11f;
	}

	FORCEINLINE float GetMax() const
	{
		return FMath::Max(FMath::Max(FMath::Max(R, G), B), A);
	}

	FORCEINLINE float GetMin() const
	{
		return FMath::Min(FMath::Min(FMath::Min(R, G), B), A);
	}

	FORCEINLINE bool IsAlmostBlack() const
	{
		return FMath::Square(R) < DELTA && FMath::Square(G) < DELTA && FMath::Square(B) < DELTA;
	}

	/** Euclidean distance, alpha included (UE: Dist). */
	static FORCEINLINE float Dist(const FLinearColor& V1, const FLinearColor& V2)
	{
		return FMath::Sqrt(FMath::Square(V2.R - V1.R) + FMath::Square(V2.G - V1.G) + FMath::Square(V2.B - V1.B) +
			FMath::Square(V2.A - V1.A));
	}

	/** "(R=%f,G=%f,B=%f,A=%f)" (UE). */
	FString ToString() const;
	bool InitFromString(const FString& InSourceString);
};

FORCEINLINE FLinearColor operator*(float Scalar, const FLinearColor& Color)
{
	return Color.operator*(Scalar);
}

/** 8-bit color stored B, G, R, A in memory (UE: FColor). Usually sRGB. */
struct alignas(4) CORE_API FColor
{
#if PLATFORM_LITTLE_ENDIAN
	uint8 B;
	uint8 G;
	uint8 R;
	uint8 A;
#else
	uint8 A;
	uint8 R;
	uint8 G;
	uint8 B;
#endif

	static const FColor White;
	static const FColor Black;
	static const FColor Transparent;
	static const FColor Red;
	static const FColor Green;
	static const FColor Blue;
	static const FColor Yellow;
	static const FColor Cyan;
	static const FColor Magenta;
	static const FColor Orange;
	static const FColor Purple;
	static const FColor Turquoise;
	static const FColor Silver;
	static const FColor Emerald;

	/** Uninitialised (UE). */
	FColor() = default;

	explicit FORCEINLINE FColor(EForceInit)
	{
		R = G = B = A = 0;
	}

	constexpr FORCEINLINE FColor(uint8 InR, uint8 InG, uint8 InB, uint8 InA = 255)
#if PLATFORM_LITTLE_ENDIAN
		: B(InB)
		, G(InG)
		, R(InR)
		, A(InA)
#else
		: A(InA)
		, R(InR)
		, G(InG)
		, B(InB)
#endif
	{
	}

	/** From the packed ARGB value (UE: FColor(uint32)). */
	explicit FColor(uint32 InColor);

	/** Packed ARGB value, A in the top byte (UE: DWColor; returned by value in Leon). */
	uint32 DWColor() const;

	FORCEINLINE bool operator==(const FColor& C) const
	{
		return R == C.R && G == C.G && B == C.B && A == C.A;
	}

	FORCEINLINE bool operator!=(const FColor& C) const
	{
		return !(*this == C);
	}

	/** Saturating add (UE: operator+=). */
	FORCEINLINE void operator+=(const FColor& C)
	{
		R = (uint8)FMath::Min((int32)R + (int32)C.R, 255);
		G = (uint8)FMath::Min((int32)G + (int32)C.G, 255);
		B = (uint8)FMath::Min((int32)B + (int32)C.B, 255);
		A = (uint8)FMath::Min((int32)A + (int32)C.A, 255);
	}

	/** Each byte divided by 255, no curve (UE: ReinterpretAsLinear). */
	FLinearColor ReinterpretAsLinear() const;

	/** "#RGB", "#RRGGBB" or "#RRGGBBAA", the '#' optional (UE: FromHex). */
	static FColor FromHex(const FString& HexString);

	/** Random saturated color (UE: MakeRandomColor). */
	static FColor MakeRandomColor();

	/** Red at 0, yellow at 0.5, green at 1 (UE: MakeRedToGreenColorFromScalar). */
	static FColor MakeRedToGreenColorFromScalar(float Scalar);

	FORCEINLINE FColor WithAlpha(uint8 Alpha) const
	{
		return FColor(R, G, B, Alpha);
	}

	/** "RRGGBBAA" (UE: ToHex). */
	FString ToHex() const;

	/** "(R=%i,G=%i,B=%i,A=%i)" (UE). */
	FString ToString() const;
	bool InitFromString(const FString& InSourceString);
};

FORCEINLINE uint32 GetTypeHash(const FColor& Color)
{
	return Color.DWColor();
}

inline FArchive& operator<<(FArchive& Ar, FLinearColor& Color)
{
	return Ar << Color.R << Color.G << Color.B << Color.A;
}

/** Stored as the packed ARGB uint32 (UE). */
inline FArchive& operator<<(FArchive& Ar, FColor& Color)
{
	uint32 Packed = Color.DWColor();
	Ar << Packed;
	if (Ar.IsLoading())
	{
		Color = FColor(Packed);
	}
	return Ar;
}
