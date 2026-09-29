#include "GSVertexBatch.h"

FVector FGSVertexLights::Irradiance(const FVector& Position, const FVector& Normal) const
{
	FVector Light = Ambient;
	for (int32 Index = 0; Index < NumDirectional; ++Index)
	{
		const float NdL = FVector::DotProduct(Normal, -Directional[Index].Direction);
		if (NdL > 0.0f)
		{
			Light += Directional[Index].Color * NdL;
		}
	}
	for (int32 Index = 0; Index < NumPoint; ++Index)
	{
		const FPoint& PointLight = Point[Index];
		const FVector ToLight = PointLight.Position - Position;
		const float Distance = ToLight.Size();
		const float Range = FMath::Max(PointLight.Radius, 0.1f);
		if (Distance >= Range || Distance <= 0.0f)
		{
			continue;
		}
		const float NdL = FVector::DotProduct(Normal, ToLight / Distance);
		if (NdL > 0.0f)
		{
			const float Attenuation = FMath::Square(1.0f - (Distance / Range));
			Light += PointLight.Color * (NdL * Attenuation);
		}
	}
	return Light;
}

FGSVertexFog FGSVertexFog::MakeLinear(float StartDistance, float EndDistance)
{
	FGSVertexFog Fog;
	Fog.bEnabled = true;
	const float Span = FMath::Max(EndDistance - StartDistance, 1.0f);
	// F = 255 (End - w) / (End - Start): 255 at the start, 0 at the end.
	Fog.Scale = -255.0f / Span;
	Fog.Offset = 255.0f * (StartDistance + Span) / Span;
	return Fog;
}

FGSSkinMatrix FGSSkinMatrix::FromMatrix(const FMatrix& M)
{
	FGSSkinMatrix Skin;
	for (int32 Row = 0; Row < 3; ++Row)
	{
		Skin.Rows[Row][0] = M.M[Row][0];
		Skin.Rows[Row][1] = M.M[Row][1];
		Skin.Rows[Row][2] = M.M[Row][2];
		Skin.Rows[Row][3] = M.M[3][Row];
	}
	return Skin;
}

bool FGSVertexDraw::GetProgram(EGSVertexProgram& OutProgram) const
{
	if (!bLit)
	{
		OutProgram = bSkinned ? EGSVertexProgram::SkinnedUnlit : EGSVertexProgram::StaticUnlit;
		return true;
	}
	if (Lights.NumDirectional > 1 || Lights.NumPoint > MaxVU1PointLights)
	{
		return false;
	}
	OutProgram = bSkinned ? EGSVertexProgram::SkinnedLit : EGSVertexProgram::StaticLit;
	return true;
}
