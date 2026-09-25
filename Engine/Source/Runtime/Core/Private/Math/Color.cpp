#include "Math/Color.h"

#include "Math/Vector.h"
#include "Math/Vector4.h"
#include "MathStringParsing.h"

#include <cstring>

// Color constants and conversions (UE: Math/Color.cpp).

const FLinearColor FLinearColor::White(1.f, 1.f, 1.f);
const FLinearColor FLinearColor::Gray(0.5f, 0.5f, 0.5f);
const FLinearColor FLinearColor::Black(0, 0, 0);
const FLinearColor FLinearColor::Transparent(0, 0, 0, 0);
const FLinearColor FLinearColor::Red(1.f, 0, 0);
const FLinearColor FLinearColor::Green(0, 1.f, 0);
const FLinearColor FLinearColor::Blue(0, 0, 1.f);
const FLinearColor FLinearColor::Yellow(1.f, 1.f, 0);

const FColor FColor::White(255, 255, 255);
const FColor FColor::Black(0, 0, 0);
const FColor FColor::Transparent(0, 0, 0, 0);
const FColor FColor::Red(255, 0, 0);
const FColor FColor::Green(0, 255, 0);
const FColor FColor::Blue(0, 0, 255);
const FColor FColor::Yellow(255, 255, 0);
const FColor FColor::Cyan(0, 255, 255);
const FColor FColor::Magenta(255, 0, 255);
const FColor FColor::Orange(243, 156, 18);
const FColor FColor::Purple(169, 7, 228);
const FColor FColor::Turquoise(26, 188, 156);
const FColor FColor::Silver(189, 195, 199);
const FColor FColor::Emerald(46, 204, 113);

// The IEC 61966-2-1 sRGB curve for each byte value.
const float FLinearColor::sRGBToLinearTable[256] = {
	0.f,
	0.000303526984f,
	0.000607053967f,
	0.000910580951f,
	0.00121410793f,
	0.00151763492f,
	0.0018211619f,
	0.00212468888f,
	0.00242821587f,
	0.00273174285f,
	0.00303526984f,
	0.00334653576f,
	0.00367650732f,
	0.00402471702f,
	0.00439144204f,
	0.00477695348f,
	0.0051815167f,
	0.00560539162f,
	0.00604883302f,
	0.00651209079f,
	0.00699541019f,
	0.00749903204f,
	0.00802319299f,
	0.00856812562f,
	0.0091340587f,
	0.00972121732f,
	0.010329823f,
	0.010960094f,
	0.0116122452f,
	0.0122864884f,
	0.0129830323f,
	0.013702083f,
	0.0144438436f,
	0.0152085144f,
	0.0159962934f,
	0.0168073758f,
	0.0176419545f,
	0.0185002201f,
	0.019382361f,
	0.0202885631f,
	0.0212190104f,
	0.0221738848f,
	0.0231533662f,
	0.0241576324f,
	0.0251868596f,
	0.0262412219f,
	0.0273208916f,
	0.0284260395f,
	0.0295568344f,
	0.0307134437f,
	0.0318960331f,
	0.0331047666f,
	0.0343398068f,
	0.0356013149f,
	0.0368894504f,
	0.0382043716f,
	0.0395462353f,
	0.0409151969f,
	0.0423114106f,
	0.0437350293f,
	0.0451862044f,
	0.0466650863f,
	0.0481718242f,
	0.049706566f,
	0.0512694584f,
	0.052860647f,
	0.0544802764f,
	0.05612849f,
	0.0578054302f,
	0.0595112382f,
	0.0612460542f,
	0.0630100177f,
	0.0648032667f,
	0.0666259386f,
	0.0684781698f,
	0.0703600957f,
	0.0722718507f,
	0.0742135684f,
	0.0761853815f,
	0.0781874218f,
	0.0802198203f,
	0.0822827071f,
	0.0843762115f,
	0.086500462f,
	0.0886555863f,
	0.0908417112f,
	0.0930589628f,
	0.0953074666f,
	0.0975873471f,
	0.0998987282f,
	0.102241733f,
	0.104616484f,
	0.107023103f,
	0.109461711f,
	0.111932428f,
	0.114435374f,
	0.116970668f,
	0.119538428f,
	0.122138772f,
	0.124771818f,
	0.12743768f,
	0.130136477f,
	0.132868322f,
	0.13563333f,
	0.138431615f,
	0.141263291f,
	0.144128471f,
	0.147027266f,
	0.14995979f,
	0.152926152f,
	0.155926464f,
	0.158960835f,
	0.162029376f,
	0.165132195f,
	0.1682694f,
	0.171441101f,
	0.174647404f,
	0.177888416f,
	0.181164244f,
	0.184474995f,
	0.187820772f,
	0.191201683f,
	0.19461783f,
	0.19806932f,
	0.201556254f,
	0.205078736f,
	0.20863687f,
	0.212230757f,
	0.2158605f,
	0.2195262f,
	0.223227957f,
	0.226965874f,
	0.230740049f,
	0.234550582f,
	0.238397574f,
	0.242281122f,
	0.246201327f,
	0.250158285f,
	0.254152094f,
	0.258182853f,
	0.262250658f,
	0.266355605f,
	0.270497791f,
	0.274677312f,
	0.278894263f,
	0.28314874f,
	0.287440838f,
	0.29177065f,
	0.296138271f,
	0.300543794f,
	0.304987314f,
	0.309468923f,
	0.313988713f,
	0.318546778f,
	0.323143209f,
	0.327778098f,
	0.332451536f,
	0.337163615f,
	0.341914425f,
	0.346704056f,
	0.3515326f,
	0.356400144f,
	0.36130678f,
	0.366252596f,
	0.37123768f,
	0.376262123f,
	0.381326011f,
	0.386429434f,
	0.391572478f,
	0.396755231f,
	0.40197778f,
	0.407240212f,
	0.412542613f,
	0.417885071f,
	0.42326767f,
	0.428690497f,
	0.434153636f,
	0.439657174f,
	0.445201195f,
	0.450785783f,
	0.456411023f,
	0.462077f,
	0.467783796f,
	0.473531496f,
	0.479320183f,
	0.48514994f,
	0.49102085f,
	0.496932995f,
	0.502886458f,
	0.508881321f,
	0.514917665f,
	0.520995573f,
	0.527115126f,
	0.533276404f,
	0.539479489f,
	0.545724461f,
	0.552011402f,
	0.55834039f,
	0.564711506f,
	0.571124829f,
	0.57758044f,
	0.584078418f,
	0.590618841f,
	0.597201788f,
	0.603827339f,
	0.610495571f,
	0.617206562f,
	0.623960392f,
	0.630757136f,
	0.637596874f,
	0.644479682f,
	0.651405637f,
	0.658374817f,
	0.665387298f,
	0.672443157f,
	0.67954247f,
	0.686685312f,
	0.693871761f,
	0.701101892f,
	0.70837578f,
	0.715693501f,
	0.723055129f,
	0.73046074f,
	0.737910409f,
	0.74540421f,
	0.752942217f,
	0.760524505f,
	0.768151147f,
	0.775822218f,
	0.783537792f,
	0.79129794f,
	0.799102738f,
	0.806952258f,
	0.814846572f,
	0.822785754f,
	0.830769877f,
	0.838799012f,
	0.846873232f,
	0.854992608f,
	0.863157213f,
	0.871367119f,
	0.879622397f,
	0.887923118f,
	0.896269353f,
	0.904661174f,
	0.913098652f,
	0.921581856f,
	0.930110858f,
	0.938685728f,
	0.947306537f,
	0.955973353f,
	0.964686248f,
	0.97344529f,
	0.98225055f,
	0.991102097f,
	1.f,
};

// FLinearColor -------------------------------------------------------------------------------------------------------

FLinearColor::FLinearColor(const FColor& Color)
{
	R = sRGBToLinearTable[Color.R];
	G = sRGBToLinearTable[Color.G];
	B = sRGBToLinearTable[Color.B];
	A = float(Color.A) * (1.0f / 255.0f);
}

FLinearColor::FLinearColor(const FVector& Vector)
	: R(Vector.X)
	, G(Vector.Y)
	, B(Vector.Z)
	, A(1.0f)
{
}

FLinearColor::FLinearColor(const FVector4& Vector)
	: R(Vector.X)
	, G(Vector.Y)
	, B(Vector.Z)
	, A(Vector.W)
{
}

FLinearColor FLinearColor::FromSRGBColor(const FColor& Color)
{
	return FLinearColor(Color);
}

FLinearColor FLinearColor::FromPow22Color(const FColor& Color)
{
	FLinearColor LinearColor;
	LinearColor.R = FMath::Pow(float(Color.R) * (1.0f / 255.0f), 2.2f);
	LinearColor.G = FMath::Pow(float(Color.G) * (1.0f / 255.0f), 2.2f);
	LinearColor.B = FMath::Pow(float(Color.B) * (1.0f / 255.0f), 2.2f);
	LinearColor.A = float(Color.A) * (1.0f / 255.0f);
	return LinearColor;
}

FColor FLinearColor::ToFColor(bool bSRGB) const
{
	float FloatR = FMath::Clamp(R, 0.0f, 1.0f);
	float FloatG = FMath::Clamp(G, 0.0f, 1.0f);
	float FloatB = FMath::Clamp(B, 0.0f, 1.0f);
	const float FloatA = FMath::Clamp(A, 0.0f, 1.0f);

	if (bSRGB)
	{
		FloatR = FloatR <= 0.0031308f ? FloatR * 12.92f : FMath::Pow(FloatR, 1.0f / 2.4f) * 1.055f - 0.055f;
		FloatG = FloatG <= 0.0031308f ? FloatG * 12.92f : FMath::Pow(FloatG, 1.0f / 2.4f) * 1.055f - 0.055f;
		FloatB = FloatB <= 0.0031308f ? FloatB * 12.92f : FMath::Pow(FloatB, 1.0f / 2.4f) * 1.055f - 0.055f;
	}

	FColor Ret;
	Ret.A = (uint8)FMath::FloorToInt(FloatA * 255.999f);
	Ret.R = (uint8)FMath::FloorToInt(FloatR * 255.999f);
	Ret.G = (uint8)FMath::FloorToInt(FloatG * 255.999f);
	Ret.B = (uint8)FMath::FloorToInt(FloatB * 255.999f);
	return Ret;
}

FColor FLinearColor::Quantize() const
{
	return FColor((uint8)FMath::Clamp(FMath::TruncToInt(R * 255.f), 0, 255),
		(uint8)FMath::Clamp(FMath::TruncToInt(G * 255.f), 0, 255),
		(uint8)FMath::Clamp(FMath::TruncToInt(B * 255.f), 0, 255),
		(uint8)FMath::Clamp(FMath::TruncToInt(A * 255.f), 0, 255));
}

FColor FLinearColor::QuantizeRound() const
{
	return FColor((uint8)FMath::Clamp(FMath::RoundToInt(R * 255.f), 0, 255),
		(uint8)FMath::Clamp(FMath::RoundToInt(G * 255.f), 0, 255),
		(uint8)FMath::Clamp(FMath::RoundToInt(B * 255.f), 0, 255),
		(uint8)FMath::Clamp(FMath::RoundToInt(A * 255.f), 0, 255));
}

FLinearColor FLinearColor::Desaturate(float Desaturation) const
{
	const float Lum = ComputeLuminance();
	return FMath::Lerp(*this, FLinearColor(Lum, Lum, Lum, 0), Desaturation);
}

FLinearColor FLinearColor::LinearRGBToHSV() const
{
	const float RGBMin = FMath::Min3(R, G, B);
	const float RGBMax = FMath::Max3(R, G, B);
	const float RGBRange = RGBMax - RGBMin;

	const float Hue = (RGBMax == RGBMin ? 0.0f
			: RGBMax == R               ? FMath::Fmod((((G - B) / RGBRange) * 60.0f) + 360.0f, 360.0f)
			: RGBMax == G               ? (((B - R) / RGBRange) * 60.0f) + 120.0f
			: RGBMax == B               ? (((R - G) / RGBRange) * 60.0f) + 240.0f
										: 0.0f);

	const float Saturation = (RGBMax == 0.0f ? 0.0f : RGBRange / RGBMax);
	const float Value = RGBMax;

	// In the new color, R = H, G = S, B = V, A = A.
	return FLinearColor(Hue, Saturation, Value, A);
}

FLinearColor FLinearColor::HSVToLinearRGB() const
{
	// In this color, R = H, G = S, B = V.
	const float Hue = R;
	const float Saturation = G;
	const float Value = B;

	const float HDiv60 = Hue / 60.0f;
	const float HDiv60Floor = FMath::FloorToFloat(HDiv60);
	const float HDiv60Fraction = HDiv60 - HDiv60Floor;

	const float RGBValues[4] = {
		Value,
		Value * (1.0f - Saturation),
		Value * (1.0f - (HDiv60Fraction * Saturation)),
		Value * (1.0f - ((1.0f - HDiv60Fraction) * Saturation)),
	};
	const uint32 RGBSwizzle[6][3] = {
		{0, 3, 1},
		{2, 0, 1},
		{1, 0, 3},
		{1, 2, 0},
		{3, 1, 0},
		{0, 1, 2},
	};
	const uint32 SwizzleIndex = ((uint32)HDiv60Floor) % 6;

	return FLinearColor(RGBValues[RGBSwizzle[SwizzleIndex][0]], RGBValues[RGBSwizzle[SwizzleIndex][1]],
		RGBValues[RGBSwizzle[SwizzleIndex][2]], A);
}

FLinearColor FLinearColor::MakeFromHSV8(uint8 H, uint8 S, uint8 V)
{
	const FLinearColor ColorHSV(float(H) * 360.0f / 255.0f, float(S) / 255.0f, float(V) / 255.0f);
	return ColorHSV.HSVToLinearRGB();
}

FLinearColor FLinearColor::MakeRandomColor()
{
	const uint8 Hue = (uint8)(FMath::FRand() * 255.f);
	return FLinearColor::MakeFromHSV8(Hue, 255, 255);
}

FLinearColor FLinearColor::LerpUsingHSV(const FLinearColor& From, const FLinearColor& To, float Progress)
{
	const FLinearColor FromHSV = From.LinearRGBToHSV();
	const FLinearColor ToHSV = To.LinearRGBToHSV();

	float FromHue = FromHSV.R;
	float ToHue = ToHSV.R;

	// Take the shortest path to the new hue.
	if (FMath::Abs(FromHue - ToHue) > 180.0f)
	{
		if (ToHue > FromHue)
		{
			FromHue += 360.0f;
		}
		else
		{
			ToHue += 360.0f;
		}
	}

	float NewHue = FMath::Lerp(FromHue, ToHue, Progress);

	NewHue = FMath::Fmod(NewHue, 360.0f);
	if (NewHue < 0.0f)
	{
		NewHue += 360.0f;
	}

	const float NewSaturation = FMath::Lerp(FromHSV.G, ToHSV.G, Progress);
	const float NewValue = FMath::Lerp(FromHSV.B, ToHSV.B, Progress);
	FLinearColor Interpolated = FLinearColor(NewHue, NewSaturation, NewValue).HSVToLinearRGB();

	Interpolated.A = FMath::Lerp(From.A, To.A, Progress);
	return Interpolated;
}

FString FLinearColor::ToString() const
{
	return FString::Printf("(R=%f,G=%f,B=%f,A=%f)", double(R), double(G), double(B), double(A));
}

bool FLinearColor::InitFromString(const FString& InSourceString)
{
	R = G = B = 0.f;
	A = 1.f;

	// The initialization is only successful if the R, G, and B values can all be parsed from the string.
	const bool bSuccessful = MathStringParsing::ParseFloat(*InSourceString, "R=", R) &&
		MathStringParsing::ParseFloat(*InSourceString, "G=", G) &&
		MathStringParsing::ParseFloat(*InSourceString, "B=", B);

	// Alpha is optional, so don't factor in its presence (or lack thereof) in determining initialization success.
	MathStringParsing::ParseFloat(*InSourceString, "A=", A);

	return bSuccessful;
}

// FVector / FVector4 from a color (declared with the vectors, defined here next to FLinearColor).

FVector::FVector(const FLinearColor& InColor)
	: X(InColor.R)
	, Y(InColor.G)
	, Z(InColor.B)
{
}

FVector4::FVector4(const FLinearColor& InColor)
	: X(InColor.R)
	, Y(InColor.G)
	, Z(InColor.B)
	, W(InColor.A)
{
}

// FColor -------------------------------------------------------------------------------------------------------------

FColor::FColor(uint32 InColor)
{
	// ARGB with A in the top byte, whatever the byte order of the members.
	A = uint8(InColor >> 24);
	R = uint8(InColor >> 16);
	G = uint8(InColor >> 8);
	B = uint8(InColor);
}

uint32 FColor::DWColor() const
{
	return (uint32(A) << 24) | (uint32(R) << 16) | (uint32(G) << 8) | uint32(B);
}

FLinearColor FColor::ReinterpretAsLinear() const
{
	return FLinearColor(R / 255.f, G / 255.f, B / 255.f, A / 255.f);
}

namespace
{
	uint8 HexDigit(TCHAR C)
	{
		if (C >= '0' && C <= '9')
		{
			return uint8(C - '0');
		}
		if (C >= 'a' && C <= 'f')
		{
			return uint8(10 + C - 'a');
		}
		if (C >= 'A' && C <= 'F')
		{
			return uint8(10 + C - 'A');
		}
		return 0;
	}
} // namespace

FColor FColor::FromHex(const FString& HexString)
{
	int32 StartIndex = (!HexString.IsEmpty() && HexString[0] == '#') ? 1 : 0;

	if (HexString.Len() == 3 + StartIndex)
	{
		const uint8 HexR = HexDigit(HexString[StartIndex++]);
		const uint8 HexG = HexDigit(HexString[StartIndex++]);
		const uint8 HexB = HexDigit(HexString[StartIndex]);

		return FColor(uint8((HexR << 4) + HexR), uint8((HexG << 4) + HexG), uint8((HexB << 4) + HexB), 255);
	}

	if (HexString.Len() == 6 + StartIndex || HexString.Len() == 8 + StartIndex)
	{
		FColor Result;

		Result.R = uint8((HexDigit(HexString[StartIndex + 0]) << 4) + HexDigit(HexString[StartIndex + 1]));
		Result.G = uint8((HexDigit(HexString[StartIndex + 2]) << 4) + HexDigit(HexString[StartIndex + 3]));
		Result.B = uint8((HexDigit(HexString[StartIndex + 4]) << 4) + HexDigit(HexString[StartIndex + 5]));
		Result.A = HexString.Len() == 8 + StartIndex
			? uint8((HexDigit(HexString[StartIndex + 6]) << 4) + HexDigit(HexString[StartIndex + 7]))
			: 255;

		return Result;
	}

	return FColor(ForceInit);
}

FColor FColor::MakeRandomColor()
{
	return FLinearColor::MakeRandomColor().ToFColor(true);
}

FColor FColor::MakeRedToGreenColorFromScalar(float Scalar)
{
	const float RedSclr = FMath::Clamp((1.0f - Scalar) / 0.5f, 0.f, 1.f);
	const float GreenSclr = FMath::Clamp((Scalar / 0.5f), 0.f, 1.f);
	const int32 RedValue = FMath::TruncToInt(255 * RedSclr);
	const int32 GreenValue = FMath::TruncToInt(255 * GreenSclr);
	const int32 BlueValue = 0;
	return FColor(uint8(RedValue), uint8(GreenValue), uint8(BlueValue));
}

FString FColor::ToHex() const
{
	return FString::Printf("%02X%02X%02X%02X", R, G, B, A);
}

FString FColor::ToString() const
{
	return FString::Printf("(R=%i,G=%i,B=%i,A=%i)", R, G, B, A);
}

bool FColor::InitFromString(const FString& InSourceString)
{
	R = G = B = 0;
	A = 255;

	// The initialization is only successful if the R, G, and B values can all be parsed from the string.
	int32 ParsedR = 0;
	int32 ParsedG = 0;
	int32 ParsedB = 0;
	int32 ParsedA = 255;
	const bool bSuccessful = MathStringParsing::ParseInt(*InSourceString, "R=", ParsedR) &&
		MathStringParsing::ParseInt(*InSourceString, "G=", ParsedG) &&
		MathStringParsing::ParseInt(*InSourceString, "B=", ParsedB);

	// Alpha is optional, so don't factor in its presence (or lack thereof) in determining initialization success.
	MathStringParsing::ParseInt(*InSourceString, "A=", ParsedA);

	R = uint8(ParsedR);
	G = uint8(ParsedG);
	B = uint8(ParsedB);
	A = uint8(ParsedA);
	return bSuccessful;
}
