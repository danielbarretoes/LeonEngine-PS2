#include "PhysicsEngine/BodySetup.h"

FBox FKBoxElem::CalcAABB(const FTransform& Transform) const
{
	const FVector HalfExtent(X * 0.5f, Y * 0.5f, Z * 0.5f);
	const FTransform ElemTransform(Rotation, Center);
	return FBox(-HalfExtent, HalfExtent).TransformBy(ElemTransform * Transform);
}

FBox FKAggregateGeom::CalcAABB(const FTransform& Transform) const
{
	FBox Box(ForceInit);
	for (const FKBoxElem& Elem : BoxElems)
	{
		Box += Elem.CalcAABB(Transform);
	}
	return Box;
}

UBodySetup::UBodySetup(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}
