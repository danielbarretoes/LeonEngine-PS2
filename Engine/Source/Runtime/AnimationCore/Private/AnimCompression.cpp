#include "AnimCompression.h"

#include "Misc/ConfigCacheIni.h"

namespace
{

	/** 1 / sqrt(2): the largest a component other than the largest of a unit quaternion can be. */
	constexpr float InverseSqrt2 = 0.70710678118654752f;
	/** A 15-bit component's largest value: even, so that 0 is a step (a rest pose's rotations stay exact). */
	constexpr int32 QuatComponentSteps = 32766;
	/** The int16 range a translation axis is spread over. */
	constexpr int32 TranslationRange = 32767;
	/** A frame number is a uint16. */
	constexpr int32 MaxFrames = 65536;

	[[nodiscard]] float GetComponent(const FQuat& Q, int32 Index)
	{
		return Index == 0 ? Q.X : Index == 1 ? Q.Y : Index == 2 ? Q.Z : Q.W;
	}

	/**
	 * Greedy key reduction over NumFrames frames: from each kept key, the farthest frame whose interpolation from it
	 * leaves every frame in between within the tolerance (Fits(From, To)) becomes the next kept key. The first and the
	 * last frames are always kept.
	 */
	template <typename FitsType>
	void ReduceKeys(int32 NumFrames, const FitsType& Fits, TArray<int32>& OutKeys)
	{
		OutKeys.Reset();
		OutKeys.Add(0);
		int32 From = 0;
		while (From < NumFrames - 1)
		{
			int32 To = From + 1;
			while (To + 1 < NumFrames && Fits(From, To + 1))
			{
				++To;
			}
			OutKeys.Add(To);
			From = To;
		}
	}

	/** Keys A and B around Frame in a run of rising frame numbers, and the weight of B. */
	void FindKeys(
		const TArray<uint16>& Frames, int32 First, int32 Num, float Frame, int32& OutA, int32& OutB, float& OutAlpha)
	{
		OutAlpha = 0.0f;
		const int32 Last = First + Num - 1;
		if (Num <= 1 || Frame <= float(Frames[First]))
		{
			OutA = First;
			OutB = First;
			return;
		}
		if (Frame >= float(Frames[Last]))
		{
			OutA = Last;
			OutB = Last;
			return;
		}
		// Frames[Low] <= Frame < Frames[High].
		int32 Low = First;
		int32 High = Last;
		while (High - Low > 1)
		{
			const int32 Middle = (Low + High) / 2;
			if (float(Frames[Middle]) <= Frame)
			{
				Low = Middle;
			}
			else
			{
				High = Middle;
			}
		}
		OutA = Low;
		OutB = High;
		OutAlpha = (Frame - float(Frames[Low])) / float(int32(Frames[High]) - int32(Frames[Low]));
	}

	[[nodiscard]] FQuat ReadRotation(const TArray<uint16>& Rotations, int32 Key)
	{
		FQuantizedQuat48 Packed;
		Packed.Words[0] = Rotations[Key * 3];
		Packed.Words[1] = Rotations[(Key * 3) + 1];
		Packed.Words[2] = Rotations[(Key * 3) + 2];
		return Packed.Dequantize();
	}

	[[nodiscard]] FVector ReadVector(const TArray<float>& Values, int32 Key)
	{
		return FVector(Values[Key * 3], Values[(Key * 3) + 1], Values[(Key * 3) + 2]);
	}

	[[nodiscard]] bool AreFramesRising(const TArray<uint16>& Frames, int32 First, int32 Num)
	{
		for (int32 Key = First + 1; Key < First + Num; ++Key)
		{
			if (Frames[Key] <= Frames[Key - 1])
			{
				return false;
			}
		}
		return true;
	}

	[[nodiscard]] bool IsRunInside(int32 First, int32 Num, int32 Size)
	{
		return First >= 0 && Num >= 0 && int64(First) + int64(Num) <= int64(Size);
	}

} // namespace

FAnimCompressionSettings FAnimCompressionSettings::Load()
{
	FAnimCompressionSettings Settings;
	if (GConfig != nullptr)
	{
		const TCHAR* const Section = TEXT("/Script/Engine.AnimationSettings");
		GConfig->GetFloat(
			Section, TEXT("RotationErrorToleranceDegrees"), Settings.RotationErrorToleranceDegrees, GEngineIni);
		GConfig->GetFloat(Section, TEXT("TranslationErrorTolerance"), Settings.TranslationErrorTolerance, GEngineIni);
		GConfig->GetFloat(Section, TEXT("ScaleErrorTolerance"), Settings.ScaleErrorTolerance, GEngineIni);
	}
	// Some tolerance is needed: the quantization alone moves a key a little.
	Settings.RotationErrorToleranceDegrees = FMath::Max(Settings.RotationErrorToleranceDegrees, 0.01f);
	Settings.TranslationErrorTolerance = FMath::Max(Settings.TranslationErrorTolerance, 0.001f);
	Settings.ScaleErrorTolerance = FMath::Max(Settings.ScaleErrorTolerance, 1.0e-5f);
	return Settings;
}

FQuantizedQuat48 FQuantizedQuat48::Quantize(const FQuat& Rotation)
{
	const FQuat Unit = Rotation.GetNormalized();
	int32 Largest = 0;
	for (int32 Index = 1; Index < 4; ++Index)
	{
		if (FMath::Abs(GetComponent(Unit, Index)) > FMath::Abs(GetComponent(Unit, Largest)))
		{
			Largest = Index;
		}
	}
	// -Q is the same rotation: the dropped component is made positive, so its square root restores it.
	const float Sign = GetComponent(Unit, Largest) < 0.0f ? -1.0f : 1.0f;
	FQuantizedQuat48 Packed;
	int32 Word = 0;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		if (Index == Largest)
		{
			continue;
		}
		const float Unsigned = ((GetComponent(Unit, Index) * Sign) * (1.0f / InverseSqrt2) * 0.5f) + 0.5f;
		Packed.Words[Word++] =
			uint16(FMath::Clamp(FMath::RoundToInt(Unsigned * float(QuatComponentSteps)), 0, QuatComponentSteps));
	}
	Packed.Words[0] = uint16(Packed.Words[0] | ((Largest & 1) << 15));
	Packed.Words[1] = uint16(Packed.Words[1] | (((Largest >> 1) & 1) << 15));
	return Packed;
}

FQuat FQuantizedQuat48::Dequantize() const
{
	const int32 Largest = ((Words[0] >> 15) & 1) | (((Words[1] >> 15) & 1) << 1);
	float Components[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	float SumSquares = 0.0f;
	int32 Word = 0;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		if (Index == Largest)
		{
			continue;
		}
		const float Unsigned = float(Words[Word++] & 0x7fff) * (1.0f / float(QuatComponentSteps));
		const float Value = ((Unsigned * 2.0f) - 1.0f) * InverseSqrt2;
		Components[Index] = Value;
		SumSquares += Value * Value;
	}
	Components[Largest] = FMath::Sqrt(FMath::Max(1.0f - SumSquares, 0.0f));
	return FQuat(Components[0], Components[1], Components[2], Components[3]);
}

FArchive& operator<<(FArchive& Ar, FCompressedBoneTrack& Track)
{
	Ar << Track.FirstRotationKey << Track.NumRotationKeys;
	Ar << Track.FirstTranslationKey << Track.NumTranslationKeys << Track.FirstTranslationValue;
	Ar << Track.FirstScaleKey << Track.NumScaleKeys;
	Ar << Track.bFloatTranslation;
	Ar << Track.TranslationScale << Track.TranslationBias;
	return Ar;
}

int32 FCompressedAnimSequence::GetNumKeys() const
{
	return RotationFrames.Num() + TranslationFrames.Num() + ScaleFrames.Num();
}

int32 FCompressedAnimSequence::GetKeyBytes() const
{
	return int32(sizeof(uint16)) *
		(RotationFrames.Num() + Rotations.Num() + TranslationFrames.Num() + ScaleFrames.Num()) +
		int32(sizeof(int16)) * Translations.Num() + int32(sizeof(float)) * (FloatTranslations.Num() + Scales.Num());
}

FTransform FCompressedAnimSequence::SampleTrack(int32 TrackIndex, float Frame) const
{
	const FCompressedBoneTrack& Track = Tracks[TrackIndex];
	int32 KeyA = 0;
	int32 KeyB = 0;
	float Alpha = 0.0f;

	FindKeys(RotationFrames, Track.FirstRotationKey, Track.NumRotationKeys, Frame, KeyA, KeyB, Alpha);
	const FQuat RotationA = ReadRotation(Rotations, KeyA);
	const FQuat Rotation = KeyA == KeyB
		? RotationA
		: FAnimCompression::InterpolateRotation(RotationA, ReadRotation(Rotations, KeyB), Alpha);

	FindKeys(TranslationFrames, Track.FirstTranslationKey, Track.NumTranslationKeys, Frame, KeyA, KeyB, Alpha);
	const int32 ValueA = Track.FirstTranslationValue + (KeyA - Track.FirstTranslationKey);
	const int32 ValueB = Track.FirstTranslationValue + (KeyB - Track.FirstTranslationKey);
	FVector TranslationA;
	FVector TranslationB;
	if (Track.bFloatTranslation)
	{
		TranslationA = ReadVector(FloatTranslations, ValueA);
		TranslationB = ReadVector(FloatTranslations, ValueB);
	}
	else
	{
		const int16* A = Translations.GetData() + (ValueA * 3);
		const int16* B = Translations.GetData() + (ValueB * 3);
		TranslationA = FVector(float(A[0]), float(A[1]), float(A[2])) * Track.TranslationScale + Track.TranslationBias;
		TranslationB = FVector(float(B[0]), float(B[1]), float(B[2])) * Track.TranslationScale + Track.TranslationBias;
	}
	const FVector Translation = TranslationA + ((TranslationB - TranslationA) * Alpha);

	FVector Scale = FVector::OneVector;
	if (Track.NumScaleKeys > 0)
	{
		FindKeys(ScaleFrames, Track.FirstScaleKey, Track.NumScaleKeys, Frame, KeyA, KeyB, Alpha);
		const FVector ScaleA = ReadVector(Scales, KeyA);
		Scale = ScaleA + ((ReadVector(Scales, KeyB) - ScaleA) * Alpha);
	}
	return FTransform(Rotation, Translation, Scale);
}

void FCompressedAnimSequence::GetBonePose(float Frame, TArray<FTransform>& OutPose) const
{
	OutPose.SetNum(Tracks.Num(), false);
	GetBonePose(Frame, TArrayView<FTransform>(OutPose));
}

void FCompressedAnimSequence::GetBonePose(float Frame, TArrayView<FTransform> OutPose) const
{
	check(OutPose.Num() == Tracks.Num());
	for (int32 TrackIndex = 0; TrackIndex < Tracks.Num(); ++TrackIndex)
	{
		OutPose[TrackIndex] = SampleTrack(TrackIndex, Frame);
	}
}

bool FCompressedAnimSequence::IsValid() const
{
	if (NumFrames <= 0 || NumFrames > MaxFrames || Rotations.Num() != RotationFrames.Num() * 3 ||
		(Translations.Num() % 3) != 0 || (FloatTranslations.Num() % 3) != 0 || Scales.Num() != ScaleFrames.Num() * 3)
	{
		return false;
	}
	for (const FCompressedBoneTrack& Track : Tracks)
	{
		const int32 NumValues = Track.bFloatTranslation ? FloatTranslations.Num() / 3 : Translations.Num() / 3;
		if (Track.NumRotationKeys < 1 || Track.NumTranslationKeys < 1 ||
			!IsRunInside(Track.FirstRotationKey, Track.NumRotationKeys, RotationFrames.Num()) ||
			!IsRunInside(Track.FirstTranslationKey, Track.NumTranslationKeys, TranslationFrames.Num()) ||
			!IsRunInside(Track.FirstTranslationValue, Track.NumTranslationKeys, NumValues) ||
			!IsRunInside(Track.FirstScaleKey, Track.NumScaleKeys, ScaleFrames.Num()) ||
			!AreFramesRising(RotationFrames, Track.FirstRotationKey, Track.NumRotationKeys) ||
			!AreFramesRising(TranslationFrames, Track.FirstTranslationKey, Track.NumTranslationKeys) ||
			!AreFramesRising(ScaleFrames, Track.FirstScaleKey, Track.NumScaleKeys))
		{
			return false;
		}
	}
	return true;
}

FArchive& operator<<(FArchive& Ar, FCompressedAnimSequence& Sequence)
{
	Ar << Sequence.NumFrames << Sequence.FrameRate << Sequence.Tracks;
	Ar << Sequence.RotationFrames << Sequence.Rotations;
	Ar << Sequence.TranslationFrames << Sequence.Translations << Sequence.FloatTranslations;
	Ar << Sequence.ScaleFrames << Sequence.Scales;
	if (Ar.IsLoading() && !Sequence.IsValid())
	{
		Ar.SetError();
		Sequence = FCompressedAnimSequence();
	}
	return Ar;
}

float FAnimCompression::GetRotationErrorDegrees(const FQuat& A, const FQuat& B)
{
	// The rotation between them, the shorter way: its angle from the vector part (exact for small angles, where an
	// arccosine of the dot product loses its precision).
	FQuat Delta = A.Inverse() * B;
	if (Delta.W < 0.0f)
	{
		Delta = Delta * -1.0f;
	}
	const float VectorLength = FMath::Sqrt((Delta.X * Delta.X) + (Delta.Y * Delta.Y) + (Delta.Z * Delta.Z));
	return FMath::RadiansToDegrees(2.0f * FMath::Atan2(VectorLength, Delta.W));
}

FQuat FAnimCompression::InterpolateRotation(const FQuat& A, const FQuat& B, float Alpha)
{
	return FQuat::FastLerp(A, B, Alpha).GetNormalized();
}

bool FAnimCompression::Compress(
	const FRawAnimSequence& Raw, const FAnimCompressionSettings& Settings, FCompressedAnimSequence& Out)
{
	Out = FCompressedAnimSequence();
	const int32 NumFrames = Raw.GetNumberOfFrames();
	if (Raw.Tracks.Num() == 0 || NumFrames <= 0 || NumFrames > MaxFrames)
	{
		return false;
	}
	Out.NumFrames = NumFrames;
	Out.FrameRate = Raw.FrameRate;

	TArray<int32> Keys;
	TArray<FQuat> SourceRotations;
	TArray<FQuat> Rotations;
	TArray<FQuantizedQuat48> PackedRotations;
	TArray<FVector> SourceVectors;
	TArray<FVector> Vectors;
	TArray<int16> PackedTranslations;
	for (const FRawAnimSequenceTrack& RawTrack : Raw.Tracks)
	{
		FCompressedBoneTrack& Track = Out.Tracks.AddDefaulted_GetRef();

		// Rotation: quantized first, so the reduction measures what the runtime will interpolate.
		SourceRotations.Reset();
		Rotations.Reset();
		PackedRotations.Reset();
		for (int32 Frame = 0; Frame < NumFrames; ++Frame)
		{
			const FQuat Source = RawTrack.GetKey(Frame).GetRotation().GetNormalized();
			const FQuantizedQuat48 Packed = FQuantizedQuat48::Quantize(Source);
			SourceRotations.Add(Source);
			PackedRotations.Add(Packed);
			Rotations.Add(Packed.Dequantize());
		}
		const float RotationTolerance = Settings.RotationErrorToleranceDegrees;
		bool bConstant = true;
		for (int32 Frame = 0; Frame < NumFrames && bConstant; ++Frame)
		{
			bConstant = GetRotationErrorDegrees(Rotations[0], SourceRotations[Frame]) <= RotationTolerance;
		}
		if (bConstant)
		{
			Keys.Reset();
			Keys.Add(0);
		}
		else
		{
			ReduceKeys(
				NumFrames,
				[&](int32 From, int32 To)
				{
					for (int32 Frame = From + 1; Frame < To; ++Frame)
					{
						const float Alpha = float(Frame - From) / float(To - From);
						const FQuat Interpolated = InterpolateRotation(Rotations[From], Rotations[To], Alpha);
						if (GetRotationErrorDegrees(Interpolated, SourceRotations[Frame]) > RotationTolerance)
						{
							return false;
						}
					}
					return true;
				},
				Keys);
		}
		Track.FirstRotationKey = Out.RotationFrames.Num();
		Track.NumRotationKeys = Keys.Num();
		for (const int32 Key : Keys)
		{
			Out.RotationFrames.Add(uint16(Key));
			Out.Rotations.Append(PackedRotations[Key].Words, 3);
		}

		// Translation: int16 with the track's scale and bias when half a step stays within a quarter of the tolerance.
		SourceVectors.Reset();
		FVector Min(TNumericLimits<float>::Max());
		FVector Max(TNumericLimits<float>::Lowest());
		for (int32 Frame = 0; Frame < NumFrames; ++Frame)
		{
			const FVector Source = RawTrack.GetKey(Frame).GetTranslation();
			SourceVectors.Add(Source);
			Min = Min.ComponentMin(Source);
			Max = Max.ComponentMax(Source);
		}
		const float TranslationTolerance = Settings.TranslationErrorTolerance;
		Track.bFloatTranslation = false;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			const float HalfExtent = (Max[Axis] - Min[Axis]) * 0.5f;
			Track.TranslationBias[Axis] = (Min[Axis] + Max[Axis]) * 0.5f;
			Track.TranslationScale[Axis] = HalfExtent / float(TranslationRange);
			Track.bFloatTranslation |= (Track.TranslationScale[Axis] * 0.5f) > (TranslationTolerance * 0.25f);
		}
		Vectors.Reset();
		PackedTranslations.Reset();
		for (const FVector& Source : SourceVectors)
		{
			if (Track.bFloatTranslation)
			{
				Vectors.Add(Source);
				continue;
			}
			FVector Decoded;
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				const float Step = Track.TranslationScale[Axis];
				const int16 Quantized = Step > 0.0f
					? int16(FMath::Clamp(FMath::RoundToInt((Source[Axis] - Track.TranslationBias[Axis]) / Step),
						  -TranslationRange, TranslationRange))
					: int16(0);
				PackedTranslations.Add(Quantized);
				Decoded[Axis] = (float(Quantized) * Step) + Track.TranslationBias[Axis];
			}
			Vectors.Add(Decoded);
		}
		if (Track.bFloatTranslation)
		{
			Track.TranslationScale = FVector::ZeroVector;
			Track.TranslationBias = FVector::ZeroVector;
		}
		bConstant = true;
		for (int32 Frame = 0; Frame < NumFrames && bConstant; ++Frame)
		{
			bConstant = FVector::Dist(Vectors[0], SourceVectors[Frame]) <= TranslationTolerance;
		}
		if (bConstant)
		{
			Keys.Reset();
			Keys.Add(0);
		}
		else
		{
			ReduceKeys(
				NumFrames,
				[&](int32 From, int32 To)
				{
					for (int32 Frame = From + 1; Frame < To; ++Frame)
					{
						const float Alpha = float(Frame - From) / float(To - From);
						const FVector Interpolated = Vectors[From] + ((Vectors[To] - Vectors[From]) * Alpha);
						if (FVector::Dist(Interpolated, SourceVectors[Frame]) > TranslationTolerance)
						{
							return false;
						}
					}
					return true;
				},
				Keys);
		}
		Track.FirstTranslationKey = Out.TranslationFrames.Num();
		Track.NumTranslationKeys = Keys.Num();
		Track.FirstTranslationValue =
			Track.bFloatTranslation ? Out.FloatTranslations.Num() / 3 : Out.Translations.Num() / 3;
		for (const int32 Key : Keys)
		{
			Out.TranslationFrames.Add(uint16(Key));
			if (Track.bFloatTranslation)
			{
				Out.FloatTranslations.Append({Vectors[Key].X, Vectors[Key].Y, Vectors[Key].Z});
			}
			else
			{
				Out.Translations.Append(PackedTranslations.GetData() + (Key * 3), 3);
			}
		}

		// Scale: only when some frame is not a unit scale.
		const float ScaleTolerance = Settings.ScaleErrorTolerance;
		SourceVectors.Reset();
		bool bUnitScale = true;
		for (int32 Frame = 0; Frame < NumFrames; ++Frame)
		{
			const FVector Source = RawTrack.GetKey(Frame).GetScale3D();
			SourceVectors.Add(Source);
			bUnitScale &= (Source - FVector::OneVector).GetAbsMax() <= ScaleTolerance;
		}
		Track.FirstScaleKey = Out.ScaleFrames.Num();
		Track.NumScaleKeys = 0;
		if (bUnitScale)
		{
			continue;
		}
		bConstant = true;
		for (int32 Frame = 0; Frame < NumFrames && bConstant; ++Frame)
		{
			bConstant = (SourceVectors[0] - SourceVectors[Frame]).GetAbsMax() <= ScaleTolerance;
		}
		if (bConstant)
		{
			Keys.Reset();
			Keys.Add(0);
		}
		else
		{
			ReduceKeys(
				NumFrames,
				[&](int32 From, int32 To)
				{
					for (int32 Frame = From + 1; Frame < To; ++Frame)
					{
						const float Alpha = float(Frame - From) / float(To - From);
						const FVector Interpolated =
							SourceVectors[From] + ((SourceVectors[To] - SourceVectors[From]) * Alpha);
						if ((Interpolated - SourceVectors[Frame]).GetAbsMax() > ScaleTolerance)
						{
							return false;
						}
					}
					return true;
				},
				Keys);
		}
		Track.NumScaleKeys = Keys.Num();
		for (const int32 Key : Keys)
		{
			Out.ScaleFrames.Add(uint16(Key));
			Out.Scales.Append({SourceVectors[Key].X, SourceVectors[Key].Y, SourceVectors[Key].Z});
		}
	}
	return Out.IsValid();
}
