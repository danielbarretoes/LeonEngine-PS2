#include "Effects/WorldEffects.h"

namespace
{

	/** The part of a life span that fades out. */
	constexpr float FadeFraction = 0.2f;

} // namespace

float FImpactMark::GetOpacity() const
{
	if (!bActive)
	{
		return 0.0f;
	}
	if (LifeSpan <= 0.0f)
	{
		return Color.A;
	}
	const float Remaining = LifeSpan - Age;
	const float FadeTime = LifeSpan * FadeFraction;
	return Color.A * FMath::Clamp(Remaining / FadeTime, 0.0f, 1.0f);
}

int32 FImpactMarkPool::AddMark(const FImpactMark& Mark)
{
	int32 Slot = INDEX_NONE;
	for (int32 Index = 0; Index < Marks.Num(); ++Index)
	{
		if (!Marks[Index].bActive)
		{
			Slot = Index;
			break;
		}
	}
	if (Slot == INDEX_NONE && Marks.Num() < MaxMarks)
	{
		Slot = Marks.AddDefaulted();
	}
	if (Slot == INDEX_NONE)
	{
		// Full: the oldest mark goes.
		Slot = 0;
		for (int32 Index = 1; Index < Marks.Num(); ++Index)
		{
			if (Marks[Index].Serial < Marks[Slot].Serial)
			{
				Slot = Index;
			}
		}
	}
	FImpactMark& NewMark = Marks[Slot];
	NewMark = Mark;
	NewMark.Normal = Mark.Normal.GetSafeNormal();
	NewMark.Age = 0.0f;
	NewMark.Serial = NextSerial++;
	NewMark.bActive = true;
	return Slot;
}

void FImpactMarkPool::Tick(float DeltaSeconds)
{
	for (FImpactMark& Mark : Marks)
	{
		if (!Mark.bActive)
		{
			continue;
		}
		Mark.Age += DeltaSeconds;
		if (Mark.LifeSpan > 0.0f && Mark.Age >= Mark.LifeSpan)
		{
			Mark.bActive = false;
		}
	}
}

void FImpactMarkPool::Clear()
{
	Marks.Reset();
}

int32 FImpactMarkPool::Num() const
{
	int32 Count = 0;
	for (const FImpactMark& Mark : Marks)
	{
		Count += Mark.bActive ? 1 : 0;
	}
	return Count;
}

float FTracer::GetOpacity() const
{
	return LifeSpan > 0.0f ? Color.A * FMath::Clamp(1.0f - (Age / LifeSpan), 0.0f, 1.0f) : 0.0f;
}

void FTracerBatch::AddTracer(const FTracer& Tracer)
{
	if (Tracers.Num() >= MaxTracers)
	{
		Tracers.RemoveAt(0);
	}
	FTracer& NewTracer = Tracers.Add_GetRef(Tracer);
	NewTracer.Age = 0.0f;
}

void FTracerBatch::Tick(float DeltaSeconds)
{
	for (int32 Index = Tracers.Num() - 1; Index >= 0; --Index)
	{
		FTracer& Tracer = Tracers[Index];
		Tracer.Age += DeltaSeconds;
		if (Tracer.Age >= Tracer.LifeSpan)
		{
			Tracers.RemoveAt(Index);
		}
	}
}
