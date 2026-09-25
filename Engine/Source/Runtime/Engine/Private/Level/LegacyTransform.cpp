#include "Level/LegacyTransform.h"

#include "LegacyGLMath.h"

namespace
{

	float SanitizeScaleComponent(float V)
	{
		constexpr float Min = 1e-4f;
		if (FMath::Abs(V) < Min)
		{
			return (V < 0.0f) ? -Min : Min;
		}
		return V;
	}

} // namespace

FMatrix FLegacyTransform::ModelMatrix() const
{
	const FVector SafeScale(
		SanitizeScaleComponent(Scale.X), SanitizeScaleComponent(Scale.Y), SanitizeScaleComponent(Scale.Z));
	FMatrix Model = FMatrix::Identity;
	Model = LegacyGL::Translate(Model, Position);
	Model = LegacyGL::Rotate(Model, FMath::DegreesToRadians(RotationDegrees.X), FVector(1.0f, 0.0f, 0.0f));
	Model = LegacyGL::Rotate(Model, FMath::DegreesToRadians(RotationDegrees.Y), FVector(0.0f, 1.0f, 0.0f));
	Model = LegacyGL::Rotate(Model, FMath::DegreesToRadians(RotationDegrees.Z), FVector(0.0f, 0.0f, 1.0f));
	Model = LegacyGL::Scale(Model, SafeScale);
	return Model;
}
