#pragma once

#include "CoreMinimal.h"

/**
 * A mark a hit leaves on a surface: a small square lying on the surface, which darkens what is under it (Leon's impact
 * marks; UE puts a UDecalComponent there, SpawnDecalAtLocation, and projects it in its deferred passes).
 */
struct ENGINE_API FImpactMark
{
	/** The point on the surface, world cm. */
	FVector Location = FVector::ZeroVector;
	/** The surface's unit normal: the mark lies across it. */
	FVector Normal = FVector(0.0f, 0.0f, 1.0f);
	/** The side of the square, cm. */
	float Size = 8.0f;
	/** The tint the surface takes at the mark's centre (multiplied), and in A how strongly. */
	FLinearColor Color = FLinearColor(0.08f, 0.07f, 0.06f, 0.9f);
	/** Seconds the mark lasts; 0 lasts until the pool recycles it. The last fifth fades out. */
	float LifeSpan = 0.0f;
	/** Seconds since it was added. */
	float Age = 0.0f;
	/** The order marks were added in (the oldest has the smallest); it also turns each mark its own way. */
	uint32 Serial = 0;
	/** False for a free slot. */
	bool bActive = false;

	/** How strongly it shows now: Color.A, fading over the last fifth of a life span. */
	[[nodiscard]] float GetOpacity() const;
};

/**
 * The world's impact marks (Leon, UWorld::ImpactMarks): a pool of MaxMarks, so a long fight never grows it. A new mark
 * takes a free slot, else the oldest mark's (oldest first, UE's decal fade has no pool). The renderer draws them over
 * the opaque geometry each frame, lifted off the surface against z-fighting (depth offset); a world without marks draws
 * nothing more.
 */
class ENGINE_API FImpactMarkPool
{
public:
	/** The most marks a world keeps. */
	static constexpr int32 MaxMarks = 64;

	/** Adds a mark; returns its slot. */
	int32 AddMark(const FImpactMark& Mark);
	/** Ages the marks and frees the expired ones. */
	void Tick(float DeltaSeconds);
	/** Frees every slot. */
	void Clear();

	/** The slots, active or free, in slot order (at most MaxMarks). */
	[[nodiscard]] const TArray<FImpactMark>& GetMarks() const
	{
		return Marks;
	}
	/** How many marks are active. */
	[[nodiscard]] int32 Num() const;
	[[nodiscard]] bool IsEmpty() const
	{
		return Num() == 0;
	}

private:
	TArray<FImpactMark> Marks;
	uint32 NextSerial = 0;
};

/** A shot's streak: a bright segment facing the camera for a moment (Leon's tracers; UE uses a beam emitter). */
struct ENGINE_API FTracer
{
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	/** Added to the scene's colour (high dynamic range: above 1 glows), fading over the life span. */
	FLinearColor Color = FLinearColor(3.0f, 2.4f, 1.2f, 1.0f);
	/** Width, cm. */
	float Width = 1.5f;
	/** Seconds it shows. */
	float LifeSpan = 0.05f;
	float Age = 0.0f;

	/** How strongly it shows now: Color.A, fading linearly over the life span. */
	[[nodiscard]] float GetOpacity() const;
};

/** The world's tracers (Leon, UWorld::Tracers): at most MaxTracers (the oldest go first); Tick drops the expired. */
class ENGINE_API FTracerBatch
{
public:
	static constexpr int32 MaxTracers = 64;

	void AddTracer(const FTracer& Tracer);
	void Tick(float DeltaSeconds);
	void Clear()
	{
		Tracers.Reset();
	}

	[[nodiscard]] const TArray<FTracer>& GetTracers() const
	{
		return Tracers;
	}
	[[nodiscard]] bool IsEmpty() const
	{
		return Tracers.Num() == 0;
	}

private:
	TArray<FTracer> Tracers;
};

/**
 * A soft round sprite facing the camera, alpha blended over the scene (Leon's smoke puffs; UE: a sprite particle of an
 * emitter, SpawnEmitterAtLocation): Size cm square, the effects' round mask, Color with A its thickest opacity, which
 * it reaches in FadeInTime and loses over its last FadeOutTime seconds.
 */
struct ENGINE_API FEffectSprite
{
	FVector Location = FVector::ZeroVector;
	float Size = 100.0f;
	FLinearColor Color = FLinearColor(0.6f, 0.6f, 0.6f, 0.8f);
	/** Seconds it lasts (0: until it is removed), and its fades at the start and the end. */
	float LifeSpan = 0.0f;
	float FadeInTime = 0.0f;
	float FadeOutTime = 0.0f;
	float Age = 0.0f;
	/** Its id (FEffectSpritePool::AddSprite's), unique in its pool; 0 for a free slot. */
	uint32 Serial = 0;

	/** How strongly it shows now: Color.A through its fades. */
	[[nodiscard]] float GetOpacity() const;
};

/**
 * The world's effect sprites (Leon, UWorld::EffectSprites): at most MaxSprites (a new one takes the oldest's slot when
 * full), so a round of smoke grenades never grows it. The GS scene renderer draws them after the translucent meshes,
 * farthest first, two triangles each; a world without sprites draws nothing more.
 */
class ENGINE_API FEffectSpritePool
{
public:
	static constexpr int32 MaxSprites = 32;

	/** Adds a sprite; returns its serial (RemoveSprite's handle). */
	uint32 AddSprite(const FEffectSprite& Sprite);
	/** Removes the sprite of Serial, if it is still there. */
	void RemoveSprite(uint32 Serial);
	/** Ages the sprites and frees the expired ones. */
	void Tick(float DeltaSeconds);
	void Clear();

	/** The slots, in use (Serial != 0) or free. */
	[[nodiscard]] const TArray<FEffectSprite>& GetSprites() const
	{
		return Sprites;
	}
	/** How many sprites are in use. */
	[[nodiscard]] int32 Num() const;
	[[nodiscard]] bool IsEmpty() const
	{
		return Num() == 0;
	}

private:
	TArray<FEffectSprite> Sprites;
	uint32 NextSerial = 1;
};
