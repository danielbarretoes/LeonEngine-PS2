#include "WorldEffectsGeometry.h"

#include "Effects/WorldEffects.h"
#include "Misc/MemStack.h"

namespace
{

	/** How far a mark sits off its surface, cm (with the polygon offset, against z-fighting). */
	constexpr float MarkLift = 0.1f;

	/** The golden angle, radians: consecutive marks turn by it, so no two neighbours look alike. */
	constexpr float GoldenAngle = 2.39996323f;

	/** Two triangles over the quad Centre +- AxisU +- AxisV. */
	template <typename AllocatorType>
	void AddQuad(TArray<FWorldEffectVertex, AllocatorType>& Out, const FVector& Centre, const FVector& AxisU,
		const FVector& AxisV, const FLinearColor& Color)
	{
		const FVector Corners[4] = {
			Centre - AxisU - AxisV, Centre + AxisU - AxisV, Centre + AxisU + AxisV, Centre - AxisU + AxisV};
		const FVector2D UVs[4] = {
			FVector2D(0.0f, 0.0f), FVector2D(1.0f, 0.0f), FVector2D(1.0f, 1.0f), FVector2D(0.0f, 1.0f)};
		constexpr int32 Order[6] = {0, 1, 2, 0, 2, 3};
		for (const int32 Corner : Order)
		{
			Out.Add({Corners[Corner], UVs[Corner], Color});
		}
	}

} // namespace

void FWorldEffectsGeometry::BuildMaskTexels(TArray<uint8>& OutTexels)
{
	// A soft round spot: 1 inside a quarter of the side, down to 0 at the rim (smoothstep).
	OutTexels.SetNumZeroed(MaskSize * MaskSize * 4);
	for (int32 Y = 0; Y < MaskSize; ++Y)
	{
		for (int32 X = 0; X < MaskSize; ++X)
		{
			const float U = ((static_cast<float>(X) + 0.5f) / static_cast<float>(MaskSize)) - 0.5f;
			const float V = ((static_cast<float>(Y) + 0.5f) / static_cast<float>(MaskSize)) - 0.5f;
			const float Radius = FMath::Sqrt((U * U) + (V * V));
			const float T = FMath::Clamp((Radius - 0.25f) / 0.25f, 0.0f, 1.0f);
			const float Value = 1.0f - (T * T * (3.0f - (2.0f * T)));
			const uint8 Byte = static_cast<uint8>(FMath::RoundToInt(Value * 255.0f));
			uint8* Texel = &OutTexels[((Y * MaskSize) + X) * 4];
			Texel[0] = Byte;
			Texel[1] = Byte;
			Texel[2] = Byte;
			Texel[3] = 255;
		}
	}
}

template <typename AllocatorType>
void FWorldEffectsGeometry::BuildImpactMarkVertices(
	const FImpactMarkPool& Marks, TArray<FWorldEffectVertex, AllocatorType>& OutVertices)
{
	// Room for every mark at once: one block, not a growth per quad.
	OutVertices.Reset(Marks.GetMarks().Num() * 6);
	for (const FImpactMark& Mark : Marks.GetMarks())
	{
		const float Opacity = Mark.GetOpacity();
		if (!Mark.bActive || Opacity <= 0.0f || Mark.Size <= 0.0f)
		{
			continue;
		}
		const FVector Normal = Mark.Normal.GetSafeNormal();
		if (Normal.IsNearlyZero())
		{
			continue;
		}
		// Two axes across the normal, turned by the mark's serial.
		const FVector Reference = FMath::Abs(Normal.Z) < 0.9f ? FVector(0.0f, 0.0f, 1.0f) : FVector(1.0f, 0.0f, 0.0f);
		const FVector AxisU = FVector::CrossProduct(Normal, Reference).GetSafeNormal();
		const FVector AxisV = FVector::CrossProduct(Normal, AxisU);
		const float Angle = static_cast<float>(Mark.Serial % 64u) * GoldenAngle;
		const float Cos = FMath::Cos(Angle);
		const float Sin = FMath::Sin(Angle);
		const float Half = Mark.Size * 0.5f;
		const FVector TurnedU = ((AxisU * Cos) + (AxisV * Sin)) * Half;
		const FVector TurnedV = ((AxisV * Cos) - (AxisU * Sin)) * Half;
		const FLinearColor Color(Mark.Color.R, Mark.Color.G, Mark.Color.B, Opacity);
		AddQuad(OutVertices, Mark.Location + (Normal * MarkLift), TurnedU, TurnedV, Color);
	}
}

template <typename AllocatorType>
void FWorldEffectsGeometry::BuildTracerVertices(
	const FTracerBatch& Tracers, const FVector& CameraLocation, TArray<FWorldEffectVertex, AllocatorType>& OutVertices)
{
	OutVertices.Reset(Tracers.GetTracers().Num() * 6);
	for (const FTracer& Tracer : Tracers.GetTracers())
	{
		const float Opacity = Tracer.GetOpacity();
		const FVector Along = Tracer.End - Tracer.Start;
		if (Opacity <= 0.0f || Along.IsNearlyZero())
		{
			continue;
		}
		// Across the segment and the line of sight, so the ribbon faces the camera.
		const FVector Middle = (Tracer.Start + Tracer.End) * 0.5f;
		FVector Side = FVector::CrossProduct(Along, CameraLocation - Middle).GetSafeNormal();
		if (Side.IsNearlyZero())
		{
			continue;
		}
		Side *= Tracer.Width * 0.5f;
		const FLinearColor Color(Tracer.Color.R, Tracer.Color.G, Tracer.Color.B, Opacity);
		const FVector HalfAlong = Along * 0.5f;
		// U runs along the tracer, V across it: the mask's middle column, bright at the centre line.
		const int32 First = OutVertices.Num();
		AddQuad(OutVertices, Middle, HalfAlong, Side, Color);
		for (int32 Index = First; Index < OutVertices.Num(); ++Index)
		{
			OutVertices[Index].TexCoord.X = 0.5f;
		}
	}
}

template <typename AllocatorType>
void FWorldEffectsGeometry::BuildEffectSpriteVertices(const FEffectSpritePool& Sprites, const FMatrix& ViewMatrix,
	const FVector& CameraLocation, TArray<FWorldEffectVertex, AllocatorType>& OutVertices)
{
	OutVertices.Reset(Sprites.GetSprites().Num() * 6);
	// The view's right and up in the world: the view matrix's first two columns (world to view space).
	const FVector Right = FVector(ViewMatrix.M[0][0], ViewMatrix.M[1][0], ViewMatrix.M[2][0]).GetSafeNormal();
	const FVector Up = FVector(ViewMatrix.M[0][1], ViewMatrix.M[1][1], ViewMatrix.M[2][1]).GetSafeNormal();
	// The sprites in use, farthest first (a handful: MaxSprites at most, sorted on the stack).
	TArray<const FEffectSprite*, TInlineAllocator<FEffectSpritePool::MaxSprites>> Order;
	for (const FEffectSprite& Sprite : Sprites.GetSprites())
	{
		if (Sprite.GetOpacity() > 0.0f && Sprite.Size > 0.0f)
		{
			Order.Add(&Sprite);
		}
	}
	Order.StableSort(
		[&CameraLocation](const FEffectSprite& A, const FEffectSprite& B)
		{
			return FVector::DistSquared(A.Location, CameraLocation) > FVector::DistSquared(B.Location, CameraLocation);
		});
	for (const FEffectSprite* Sprite : Order)
	{
		const float Half = Sprite->Size * 0.5f;
		const FLinearColor Color(Sprite->Color.R, Sprite->Color.G, Sprite->Color.B, Sprite->GetOpacity());
		AddQuad(OutVertices, Sprite->Location, Right * Half, Up * Half, Color);
	}
}

bool FWorldEffectsGeometry::PlaceBlobShadow(
	const FBox& Bounds, const FVector& FloorPoint, const FVector& FloorNormal, FBlobShadow& OutShadow)
{
	const FVector Normal = FloorNormal.GetSafeNormal();
	// A wall or a steep ramp is no floor: the shadow would stand up.
	if (!Bounds.IsValid || Normal.Z < 0.5f)
	{
		return false;
	}
	// Down the vertical from the bounds' centre onto the floor's plane.
	const FVector Center = Bounds.GetCenter();
	const float Drop = FVector::DotProduct(Center - FloorPoint, Normal) / Normal.Z;
	OutShadow.Center = Center - FVector(0.0f, 0.0f, Drop);
	OutShadow.Normal = Normal;
	const float Height = FMath::Max(Bounds.Min.Z - OutShadow.Center.Z, 0.0f);
	OutShadow.Opacity = BlobShadowOpacity * (1.0f - FMath::Clamp(Height / BlobShadowFadeHeight, 0.0f, 1.0f));
	const FVector Extent = Bounds.GetExtent();
	OutShadow.HalfSize = FMath::Max(Extent.X, Extent.Y) * BlobShadowSizeScale;
	return OutShadow.Opacity > 0.0f && OutShadow.HalfSize > 0.0f;
}

template <typename AllocatorType>
void FWorldEffectsGeometry::AddBlobShadowVertices(
	const FBlobShadow& Shadow, TArray<FWorldEffectVertex, AllocatorType>& OutVertices)
{
	// Two axes across the floor's normal; black, as strong as the opacity through the mask's round spot.
	const FVector Reference =
		FMath::Abs(Shadow.Normal.X) < 0.9f ? FVector(1.0f, 0.0f, 0.0f) : FVector(0.0f, 1.0f, 0.0f);
	const FVector AxisU = FVector::CrossProduct(Shadow.Normal, Reference).GetSafeNormal() * Shadow.HalfSize;
	const FVector AxisV = FVector::CrossProduct(Shadow.Normal, AxisU.GetSafeNormal()) * Shadow.HalfSize;
	AddQuad(OutVertices, Shadow.Center + (Shadow.Normal * MarkLift), AxisU, AxisV,
		FLinearColor(0.0f, 0.0f, 0.0f, Shadow.Opacity));
}

// The heap's arrays (the tests) and the frame's stack's (the scene renderer).
template void FWorldEffectsGeometry::AddBlobShadowVertices(const FBlobShadow&, TArray<FWorldEffectVertex>&);
template void FWorldEffectsGeometry::AddBlobShadowVertices(
	const FBlobShadow&, TArray<FWorldEffectVertex, TMemStackAllocator<>>&);
template void FWorldEffectsGeometry::BuildImpactMarkVertices(const FImpactMarkPool&, TArray<FWorldEffectVertex>&);
template void FWorldEffectsGeometry::BuildImpactMarkVertices(
	const FImpactMarkPool&, TArray<FWorldEffectVertex, TMemStackAllocator<>>&);
template void FWorldEffectsGeometry::BuildTracerVertices(
	const FTracerBatch&, const FVector&, TArray<FWorldEffectVertex>&);
template void FWorldEffectsGeometry::BuildTracerVertices(
	const FTracerBatch&, const FVector&, TArray<FWorldEffectVertex, TMemStackAllocator<>>&);
template void FWorldEffectsGeometry::BuildEffectSpriteVertices(
	const FEffectSpritePool&, const FMatrix&, const FVector&, TArray<FWorldEffectVertex>&);
template void FWorldEffectsGeometry::BuildEffectSpriteVertices(
	const FEffectSpritePool&, const FMatrix&, const FVector&, TArray<FWorldEffectVertex, TMemStackAllocator<>>&);
