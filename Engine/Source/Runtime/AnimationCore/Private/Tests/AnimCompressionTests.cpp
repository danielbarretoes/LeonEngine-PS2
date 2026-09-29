#include "AnimCompression.h"
#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "SkeletalAnimation.h"

#if WITH_DEV_AUTOMATION_TESTS

// The compressed animation keys (Docs/PLANS/ps2-shipping.md N21): 48-bit rotations, int16 translations with a scale and
// bias, error-bounded key reduction at 30 Hz, and sampling back to local transforms.

namespace
{

	/**
	 * A 90-frame clip of three bones: a smooth swing and a sweeping translation (the root sweeps 30 m, beyond int16 at
	 * 0.05 cm), a constant bone, and a bone that scales.
	 */
	FRawAnimSequence MakeClip()
	{
		FRawAnimSequence Raw;
		Raw.FrameRate = 30.0f;
		constexpr int32 NumFrames = 90;
		Raw.SequenceLength = float(NumFrames - 1) / 30.0f;
		Raw.Tracks.SetNum(3);
		for (int32 Frame = 0; Frame < NumFrames; ++Frame)
		{
			const float Time = float(Frame) / 30.0f;
			FRawAnimSequenceTrack& Root = Raw.Tracks[0];
			Root.RotKeys.Add(FQuat(FVector(0.0f, 0.0f, 1.0f), FMath::Sin(Time * 2.0f) * 1.2f));
			Root.PosKeys.Add(FVector(Time * 1000.0f, FMath::Sin(Time * 3.0f) * 40.0f, 90.0f));
			Root.ScaleKeys.Add(FVector::OneVector);
			FRawAnimSequenceTrack& Still = Raw.Tracks[1];
			Still.RotKeys.Add(FQuat(FVector(1.0f, 0.0f, 0.0f), 0.3f));
			Still.PosKeys.Add(FVector(0.0f, 12.5f, 0.0f));
			Still.ScaleKeys.Add(FVector::OneVector);
			FRawAnimSequenceTrack& Grow = Raw.Tracks[2];
			Grow.RotKeys.Add(FQuat(FVector(0.0f, 1.0f, 0.0f), Time));
			Grow.PosKeys.Add(FVector(20.0f, 0.0f, FMath::Cos(Time * 4.0f) * 5.0f));
			Grow.ScaleKeys.Add(FVector(1.0f + (Time * 0.25f)));
		}
		return Raw;
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimQuantizedQuatTest, "System.AnimationCore.Compression.QuantizedQuat48",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAnimQuantizedQuatTest::RunTest(const FString& Parameters)
{
	// Smallest three in 48 bits: any rotation comes back within 0.01 degrees, and -Q (the same rotation) packs alike.
	FRandomStream Random(0x48);
	float Worst = 0.0f;
	for (int32 Index = 0; Index < 2000; ++Index)
	{
		const FQuat Rotation(Random.GetUnitVector(), Random.FRandRange(-PI, PI));
		const FQuat Back = FQuantizedQuat48::Quantize(Rotation).Dequantize();
		Worst = FMath::Max(Worst, FAnimCompression::GetRotationErrorDegrees(Rotation, Back));
		const FQuat Negated = FQuantizedQuat48::Quantize(Rotation * -1.0f).Dequantize();
		if (FAnimCompression::GetRotationErrorDegrees(Back, Negated) > 1.0e-3f)
		{
			AddError(FString::Printf("Rotation %d and its negation pack differently", Index));
			break;
		}
	}
	UE_LOG(LogTemp, Display, "%s", *FString::Printf("QuantizedQuat48: worst error %.5f degrees", double(Worst)));
	TestTrue("Within 0.01 degrees", Worst < 0.01f);
	TestTrue("Six bytes", sizeof(FQuantizedQuat48) == 6);
	TestTrue("The identity",
		FAnimCompression::GetRotationErrorDegrees(
			FQuantizedQuat48::Quantize(FQuat::Identity).Dequantize(), FQuat::Identity) < 1.0e-3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimKeyReductionTest, "System.AnimationCore.Compression.KeyReductionBound",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAnimKeyReductionTest::RunTest(const FString& Parameters)
{
	// Every frame of the source samples back within the tolerances (rotation 0.1 degrees, translation 0.05 cm, scale
	// 0.001), with far fewer keys than frames: a constant channel keeps one key, a unit scale none, and a translation
	// too wide for int16 at the tolerance is kept in floats.
	const FRawAnimSequence Raw = MakeClip();
	const FAnimCompressionSettings Settings;
	FCompressedAnimSequence Compressed;
	if (!TestTrue("Compressed", FAnimCompression::Compress(Raw, Settings, Compressed)))
	{
		return false;
	}
	TestEqual("Frames", Compressed.NumFrames, 90);
	TestEqual("Tracks", Compressed.GetNumTracks(), 3);
	float WorstRotation = 0.0f;
	float WorstTranslation = 0.0f;
	float WorstScale = 0.0f;
	for (int32 Track = 0; Track < 3; ++Track)
	{
		for (int32 Frame = 0; Frame < Compressed.NumFrames; ++Frame)
		{
			const FTransform Sampled = Compressed.SampleTrack(Track, float(Frame));
			const FTransform Source = Raw.Tracks[Track].GetKey(Frame);
			WorstRotation = FMath::Max(
				WorstRotation, FAnimCompression::GetRotationErrorDegrees(Sampled.GetRotation(), Source.GetRotation()));
			WorstTranslation =
				FMath::Max(WorstTranslation, FVector::Dist(Sampled.GetTranslation(), Source.GetTranslation()));
			WorstScale = FMath::Max(WorstScale, (Sampled.GetScale3D() - Source.GetScale3D()).GetAbsMax());
		}
	}
	UE_LOG(LogTemp, Display, "%s",
		*FString::Printf("Key reduction: %d keys for 3 x 3 x 90 frames, %d bytes; worst %.4f deg, %.4f cm, %.5f",
			Compressed.GetNumKeys(), Compressed.GetKeyBytes(), double(WorstRotation), double(WorstTranslation),
			double(WorstScale)));
	TestTrue("Rotations within 0.1 degrees", WorstRotation <= Settings.RotationErrorToleranceDegrees);
	TestTrue("Translations within 0.05 cm", WorstTranslation <= Settings.TranslationErrorTolerance);
	TestTrue("Scales within 0.001", WorstScale <= Settings.ScaleErrorTolerance);

	const FCompressedBoneTrack& Root = Compressed.Tracks[0];
	const FCompressedBoneTrack& Still = Compressed.Tracks[1];
	const FCompressedBoneTrack& Grow = Compressed.Tracks[2];
	TestTrue(
		"Fewer rotation keys than frames", Root.NumRotationKeys > 2 && Root.NumRotationKeys < Compressed.NumFrames);
	TestTrue("Fewer keys than half the frames", Compressed.GetNumKeys() < 3 * 3 * 90 / 2);
	TestTrue("The root's 30 m sweep is kept in floats", Root.bFloatTranslation);
	TestEqual("A constant rotation keeps one key", Still.NumRotationKeys, 1);
	TestEqual("A constant translation keeps one key", Still.NumTranslationKeys, 1);
	TestFalse("A short translation is int16", Still.bFloatTranslation || Grow.bFloatTranslation);
	TestEqual("A unit scale has no keys", Still.NumScaleKeys, 0);
	TestTrue("A linear scale keeps its ends", Grow.NumScaleKeys == 2);
	TestTrue("Far smaller than the matrices (64 bytes a bone a frame)", Compressed.GetKeyBytes() < 64 * 3 * 90 / 8);

	// Between two frames the sample moves on from one to the next.
	const FVector Half = Compressed.SampleTrack(2, 10.5f).GetTranslation();
	const FVector From = Compressed.SampleTrack(2, 10.0f).GetTranslation();
	const FVector To = Compressed.SampleTrack(2, 11.0f).GetTranslation();
	TestTrue("Halfway", Half.Equals((From + To) * 0.5f, 0.05f));

	// Looser tolerances keep fewer keys.
	FAnimCompressionSettings Loose;
	Loose.RotationErrorToleranceDegrees = 2.0f;
	Loose.TranslationErrorTolerance = 2.0f;
	FCompressedAnimSequence Smaller;
	TestTrue("Compressed loosely", FAnimCompression::Compress(Raw, Loose, Smaller));
	TestTrue("Fewer keys", Smaller.GetNumKeys() < Compressed.GetNumKeys());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimCompressionArchiveTest, "System.AnimationCore.Compression.ArchiveAndDeterminism",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAnimCompressionArchiveTest::RunTest(const FString& Parameters)
{
	// The same clip compresses to the same bytes, which load back to the same keys; damaged runs do not load.
	const FRawAnimSequence Raw = MakeClip();
	FCompressedAnimSequence First;
	FCompressedAnimSequence Second;
	TestTrue("Twice",
		FAnimCompression::Compress(Raw, FAnimCompressionSettings(), First) &&
			FAnimCompression::Compress(Raw, FAnimCompressionSettings(), Second));
	TArray<uint8> BytesA;
	TArray<uint8> BytesB;
	FMemoryWriter WriterA(BytesA);
	WriterA << First;
	FMemoryWriter WriterB(BytesB);
	WriterB << Second;
	TestTrue("The same bytes", BytesA.Num() > 0 && BytesA == BytesB);

	FCompressedAnimSequence Loaded;
	FMemoryReader Reader(BytesA);
	Reader << Loaded;
	TestFalse("Loaded", Reader.IsError());
	TestTrue("The same keys",
		Loaded.Rotations == First.Rotations && Loaded.Translations == First.Translations &&
			Loaded.FloatTranslations == First.FloatTranslations && Loaded.Scales == First.Scales);
	TestTrue("The same samples",
		Loaded.SampleTrack(0, 33.3f).GetTranslation().Equals(First.SampleTrack(0, 33.3f).GetTranslation()));

	FCompressedAnimSequence Damaged = First;
	Damaged.Tracks[1].NumRotationKeys = 1000;
	TArray<uint8> DamagedBytes;
	FMemoryWriter DamagedWriter(DamagedBytes);
	DamagedWriter << Damaged;
	FCompressedAnimSequence Refused;
	FMemoryReader DamagedReader(DamagedBytes);
	DamagedReader << Refused;
	TestTrue("A run past its keys is refused", DamagedReader.IsError() && Refused.IsEmpty());

	FRawAnimSequence Empty;
	FCompressedAnimSequence None;
	TestFalse("No tracks", FAnimCompression::Compress(Empty, FAnimCompressionSettings(), None));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
