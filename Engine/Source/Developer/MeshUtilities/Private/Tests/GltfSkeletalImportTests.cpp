#include "AnimCompression.h"
#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"
#include "CoreMinimal.h"
#include "GltfImport.h"
#include "HAL/FileManager.h"
#include "ImportCoordinateConversion.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Tests/GltfTestCube.h"

#if WITH_DEV_AUTOMATION_TESTS

// The glTF skeletal import (Docs/PLANS/ps2-shipping.md N21) against SkinnedArm.glb, written by MakeSkinnedFixture.py:
// the skeleton of its skin, the two largest weights, its sockets and embedded image, and its two animations sampled at
// 30 Hz, compressed and compared with the source's keys.

namespace
{

	const FImportCoordinateConversion& GetConversion()
	{
		static const FImportCoordinateConversion Conversion(
			EImportAxes::RightHandedYUp, FImportCoordinateConversion::CmPerMetre);
		return Conversion;
	}

	/** A rotation of Degrees about a glTF axis, in the engine's axes. */
	FQuat EngineRotation(const FVector& GltfAxis, float Degrees)
	{
		return GetConversion().ConvertRotation(FQuat(GltfAxis, FMath::DegreesToRadians(Degrees)));
	}

	/** SkinnedArm.glb's skeleton, as the mesh import reads it. */
	bool LoadArm(FAutomationTestBase& Test, FSkeletalMeshImportData& Out)
	{
		FString Error;
		const bool bLoaded = LoadSkeletalMeshFromGltf(GetGltfFixturePath(TEXT("SkinnedArm.glb")), Out, Error);
		if (!bLoaded)
		{
			Test.AddError(Error);
		}
		return bLoaded;
	}

	/** A .gltf (JSON text with a data URI buffer) in the program's Intermediate folder; its full path. */
	FString WriteTempGltf(const TCHAR* Name, const FString& Text)
	{
		const FString Path = FPaths::ConvertRelativePathToFull(FPaths::EngineIntermediateDir() / TEXT("Tests") / Name);
		(void)FFileHelper::SaveStringToFile(Text, *Path);
		return Path;
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGltfSkinImportTest, "System.MeshUtilities.GltfSkeletal.SkinAndWeights",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGltfSkinImportTest::RunTest(const FString& Parameters)
{
	// The skin's joints (listed Hand, Shoulder, Elbow) become bones parents first; the Armature node above the root is
	// part of the root's pose; at rest every skin matrix is the identity (the inverse bind matrices are the joints'
	// rest). Each vertex keeps its two largest weights, renormalized to 255.
	FSkeletalMeshImportData Arm;
	if (!LoadArm(*this, Arm))
	{
		return false;
	}
	const FReferenceSkeleton& Bones = Arm.RefSkeleton;
	if (!TestEqual("Three bones", Bones.GetNum(), 3) || !TestTrue("A valid skeleton", Bones.IsValid()))
	{
		return false;
	}
	TestTrue("Parents first",
		Bones.GetBoneName(0) == FName("Shoulder") && Bones.GetBoneName(1) == FName("Elbow") &&
			Bones.GetBoneName(2) == FName("Hand"));
	TestTrue("Parents", Bones.ParentIndices == TArray<int32>({INDEX_NONE, 0, 1}));
	// glTF (0.1, 1, 0) m is the engine's (10, 0, 100) cm: the Armature's 0.1 m and the Shoulder's 1 m.
	TestTrue("The root's pose", Bones.RefBonePose[0].GetTranslation().Equals(FVector(10.0f, 0.0f, 100.0f), 1.0e-3f));
	TestTrue("The elbow's", Bones.RefBonePose[1].GetTranslation().Equals(FVector(50.0f, 0.0f, 0.0f), 1.0e-3f));
	TArray<FMatrix> ComponentSpace;
	TArray<FMatrix> Skin;
	FAnimationRuntime::FillUpComponentSpaceTransforms(Bones, Bones.RefBonePose, ComponentSpace);
	FAnimationRuntime::GetSkinMatrices(Bones, ComponentSpace, Skin);
	for (int32 Bone = 0; Bone < 3; ++Bone)
	{
		TestTrue(*FString::Printf("Bone %d at bind", Bone), Skin[Bone].Equals(FMatrix::Identity, 1.0e-3f));
	}

	if (!TestEqual("20 vertices", Arm.Mesh.Vertices.Num(), 20) || !TestEqual("Weights", Arm.SkinWeights.Num(), 20))
	{
		return false;
	}
	TestEqual("32 triangles", Arm.Mesh.Indices.Num(), 96);
	// Ring 0's first corner: glTF (0.1, 1.05, 0.05) m.
	TestTrue("In the engine world", Arm.Mesh.Vertices[0].Position.Equals(FVector(10.0f, 5.0f, 105.0f), 1.0e-3f));
	// glTF's UVs start at the image's top left, the engine's at its bottom left (textures bottom row first): ring 1's
	// second corner, glTF (0.25, 0.25), is the engine's (0.25, 0.75) (N27).
	TestTrue("UVs turned over",
		Arm.Mesh.Vertices[0].TexCoord.Equals(FVector2D(0.0f, 1.0f), 1.0e-5f) &&
			Arm.Mesh.Vertices[5].TexCoord.Equals(FVector2D(0.25f, 0.75f), 1.0e-5f));
	struct FExpected
	{
		uint8 Bone0;
		uint8 Weight0;
		uint8 Bone1;
		uint8 Weight1;
	};
	// Bones: 0 Shoulder, 1 Elbow, 2 Hand.
	const FExpected Rings[5] = {
		{0, 255, 0, 0}, // Shoulder 1
		{0, 153, 1, 102}, // Shoulder 0.6, Elbow 0.4
		{1, 159, 0, 96}, // Elbow 0.5, Shoulder 0.3 (Hand 0.2 dropped): 0.625 / 0.375
		{1, 159, 2, 96}, // Elbow 0.5, Hand 0.3 (Shoulder 0.1 + 0.1 merged, dropped)
		{2, 255, 2, 0}, // Hand 2 (not normalized)
	};
	for (int32 Vertex = 0; Vertex < 20; ++Vertex)
	{
		const FExpected& Expected = Rings[Vertex / 4];
		const FSkinWeightInfo& Info = Arm.SkinWeights[Vertex];
		if (Info.InfluenceBones[0] != Expected.Bone0 || Info.InfluenceWeights[0] != Expected.Weight0 ||
			Info.InfluenceBones[1] != Expected.Bone1 || Info.InfluenceWeights[1] != Expected.Weight1)
		{
			AddError(FString::Printf("Vertex %d: bones %d, %d weights %d, %d", Vertex, Info.InfluenceBones[0],
				Info.InfluenceBones[1], Info.InfluenceWeights[0], Info.InfluenceWeights[1]));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGltfSocketsAndImageTest, "System.MeshUtilities.GltfSkeletal.SocketsAndEmbeddedImage",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGltfSocketsAndImageTest::RunTest(const FString& Parameters)
{
	// SOCKET_Grip under the Hand is a socket of the skeleton on its nearest joint, in the bone's space; the material's
	// base colour image lives in the .glb's binary chunk and comes as its bytes (an 8x8 PNG).
	FSkeletalMeshImportData Arm;
	if (!LoadArm(*this, Arm) || !TestEqual("One socket", Arm.Sockets.Num(), 1))
	{
		return false;
	}
	const FBoneSocketData& Grip = Arm.Sockets[0];
	TestEqual("Its name", Grip.Name, FString(TEXT("Grip")));
	TestTrue("On the hand", Grip.BoneName == FName("Hand"));
	TestTrue("Its place: glTF (0.1, 0, 0.05) m",
		Grip.RelativeTransform.GetTranslation().Equals(FVector(10.0f, 5.0f, 0.0f), 1.0e-3f));
	TestTrue("Its rotation: 90 degrees about glTF +Z",
		FAnimCompression::GetRotationErrorDegrees(
			Grip.RelativeTransform.GetRotation(), EngineRotation(FVector(0.0f, 0.0f, 1.0f), 90.0f)) < 0.01f);

	TestEqual("One slot", Arm.Mesh.MaterialSlotNames.Num(), 1);
	TestEqual("Its material", Arm.Mesh.MaterialSlotNames[0], FString(TEXT("ArmSkin")));
	TestTrue("No file", Arm.Mesh.AlbedoMapPaths[0].IsEmpty());
	const FMeshEmbeddedImage& Image = Arm.Mesh.AlbedoMapImages[0];
	TestEqual("The image's name", Image.Name, FString(TEXT("ArmSkin_D")));
	TestTrue("A PNG",
		Image.EncodedData.Num() > 8 && Image.EncodedData[1] == 'P' && Image.EncodedData[2] == 'N' &&
			Image.EncodedData[3] == 'G');
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGltfAnimationsTest, "System.MeshUtilities.GltfSkeletal.AnimationsWithinTolerance",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGltfAnimationsTest::RunTest(const FString& Parameters)
{
	// Both animations, sampled at 30 Hz against the skeleton and compressed, give back the source's keys within the
	// compression's tolerances at every frame: Wave's LINEAR rotation (slerped in the source) and translation, Grip's
	// STEP rotation (held until its next key) and LINEAR scale. A bone without a channel keeps its rest.
	FSkeletalMeshImportData Arm;
	if (!LoadArm(*this, Arm))
	{
		return false;
	}
	TArray<FRawAnimSequence> Clips;
	FString Error;
	if (!TestTrue("Loaded",
			LoadAnimSequencesFromGltf(
				GetGltfFixturePath(TEXT("SkinnedArm.glb")), Arm.RefSkeleton, FString(), Clips, Error)) ||
		!TestEqual("Two clips", Clips.Num(), 2))
	{
		AddError(Error);
		return false;
	}
	TestTrue("Named after the animations", Clips[0].Name == FName("Wave") && Clips[1].Name == FName("Grip"));
	const FAnimCompressionSettings Settings;
	const float RotationTolerance = Settings.RotationErrorToleranceDegrees + 1.0e-3f;
	const float TranslationTolerance = Settings.TranslationErrorTolerance + 1.0e-4f;

	FCompressedAnimSequence Wave;
	if (TestTrue("Wave compressed", FAnimCompression::Compress(Clips[0], Settings, Wave)))
	{
		TestEqual("31 frames (1 s at 30 Hz)", Wave.NumFrames, 31);
		TestEqual("1 s", Clips[0].SequenceLength, 1.0f, 1.0e-5f);
		for (int32 Frame = 0; Frame < Wave.NumFrames; ++Frame)
		{
			const float Time = float(Frame) / 30.0f;
			// Elbow: 0 -> 90 -> 0 degrees about glTF +Z; Shoulder (with the Armature's 0.1 m): up 0.2 m over 1 s.
			const float Degrees = Time <= 0.5f ? 90.0f * (Time / 0.5f) : 90.0f * ((1.0f - Time) / 0.5f);
			const FQuat ElbowSource = EngineRotation(FVector(0.0f, 0.0f, 1.0f), Degrees);
			const FVector ShoulderSource(10.0f, 0.0f, 100.0f + (20.0f * Time));
			const float ElbowError =
				FAnimCompression::GetRotationErrorDegrees(Wave.SampleTrack(1, float(Frame)).GetRotation(), ElbowSource);
			const float ShoulderError =
				FVector::Dist(Wave.SampleTrack(0, float(Frame)).GetTranslation(), ShoulderSource);
			if (ElbowError > RotationTolerance || ShoulderError > TranslationTolerance)
			{
				AddError(FString::Printf(
					"Wave, frame %d: %.4f degrees, %.4f cm", Frame, double(ElbowError), double(ShoulderError)));
				break;
			}
		}
		TestTrue("The hand keeps its rest",
			Wave.SampleTrack(2, 12.0f).GetTranslation().Equals(FVector(40.0f, 0.0f, 0.0f), 0.05f));
		TestTrue("Fewer keys than frames", Wave.GetNumKeys() < 3 * 3 * Wave.NumFrames);
	}

	FCompressedAnimSequence Grip;
	if (TestTrue("Grip compressed", FAnimCompression::Compress(Clips[1], Settings, Grip)))
	{
		// STEP: identity until 0.5 s, 45 degrees about glTF +X until 1 s, identity at 1 s.
		const FQuat Held = EngineRotation(FVector(1.0f, 0.0f, 0.0f), 45.0f);
		const auto HandError = [&Grip](float Frame, const FQuat& Expected)
		{ return FAnimCompression::GetRotationErrorDegrees(Grip.SampleTrack(2, Frame).GetRotation(), Expected); };
		TestTrue("Before the step", HandError(7.0f, FQuat::Identity) < RotationTolerance);
		TestTrue("Just before", HandError(14.0f, FQuat::Identity) < RotationTolerance);
		TestTrue("On the key", HandError(15.0f, Held) < RotationTolerance);
		TestTrue("Held", HandError(22.0f, Held) < RotationTolerance && HandError(29.0f, Held) < RotationTolerance);
		TestTrue("The last key", HandError(30.0f, FQuat::Identity) < RotationTolerance);
		TestEqual("LINEAR scale halfway", Grip.SampleTrack(2, 15.0f).GetScale3D().X, 1.25f, 0.002f);
		TestTrue("A scale channel", Grip.Tracks[2].NumScaleKeys >= 2);
		TestEqual("No scale on the shoulder", Grip.Tracks[0].NumScaleKeys, 0);
	}

	// One animation by name.
	TArray<FRawAnimSequence> Only;
	TestTrue("Grip alone",
		LoadAnimSequencesFromGltf(
			GetGltfFixturePath(TEXT("SkinnedArm.glb")), Arm.RefSkeleton, TEXT("Grip"), Only, Error) &&
			Only.Num() == 1 && Only[0].Name == FName("Grip"));
	TestFalse("A missing name",
		LoadAnimSequencesFromGltf(
			GetGltfFixturePath(TEXT("SkinnedArm.glb")), Arm.RefSkeleton, TEXT("Dance"), Only, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGltfAnimationRejectsTest, "System.MeshUtilities.GltfSkeletal.Rejections",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGltfAnimationRejectsTest::RunTest(const FString& Parameters)
{
	// CUBICSPLINE keys are refused with a clear error; so is a skeleton whose bones do not match the file's (names,
	// order, parents), and a file without a skin for a skeletal mesh.
	FString Base64;
	for (int32 Group = 0; Group < 26; ++Group)
	{
		Base64 += TEXT("AAAA");
	}
	Base64 += TEXT("AAA=");
	const FString Cubic = WriteTempGltf(TEXT("CubicSpline.gltf"),
		TEXT("{\"asset\":{\"version\":\"2.0\"},\"buffers\":[{\"byteLength\":80,"
			 "\"uri\":\"data:application/octet-stream;base64,") +
			Base64 +
			TEXT("\"}],\"bufferViews\":[{\"buffer\":0,\"byteLength\":8},{\"buffer\":0,\"byteOffset\":8,"
				 "\"byteLength\":72}],"
				 "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":2,\"type\":\"SCALAR\",\"min\":[0],"
				 "\"max\":[0]},{\"bufferView\":1,\"componentType\":5126,\"count\":6,\"type\":\"VEC3\"}],"
				 "\"nodes\":[{\"name\":\"Bone\"}],\"scenes\":[{\"nodes\":[0]}],\"scene\":0,"
				 "\"animations\":[{\"name\":\"Bounce\",\"samplers\":[{\"input\":0,\"output\":1,"
				 "\"interpolation\":\"CUBICSPLINE\"}],\"channels\":[{\"sampler\":0,\"target\":{\"node\":0,"
				 "\"path\":\"translation\"}}]}]}"));
	FReferenceSkeleton OneBone;
	OneBone.BoneNames = {FName("Bone")};
	OneBone.ParentIndices = {INDEX_NONE};
	OneBone.RefBonePose = {FTransform::Identity};
	OneBone.InverseBindPose = {FMatrix::Identity};
	TArray<FRawAnimSequence> Clips;
	FString Error;
	TestFalse("CUBICSPLINE", LoadAnimSequencesFromGltf(Cubic, OneBone, FString(), Clips, Error));
	TestTrue("Says so", Error.Contains(TEXT("CUBICSPLINE")) && Error.Contains(TEXT("Bounce")));
	TestEqual("No clips", Clips.Num(), 0);

	FSkeletalMeshImportData Arm;
	if (LoadArm(*this, Arm))
	{
		FReferenceSkeleton Reordered = Arm.RefSkeleton;
		Swap(Reordered.BoneNames[1], Reordered.BoneNames[2]);
		TestFalse("Bones in another order",
			LoadAnimSequencesFromGltf(GetGltfFixturePath(TEXT("SkinnedArm.glb")), Reordered, FString(), Clips, Error));
		TestTrue("Explained", Error.Contains(TEXT("do not match")));
		TestFalse("Another skeleton",
			LoadAnimSequencesFromGltf(GetGltfFixturePath(TEXT("SkinnedArm.glb")), OneBone, FString(), Clips, Error));
		TestTrue("Explained", Error.Contains(TEXT("do not match")));
	}

	FSkeletalMeshImportData NoSkin;
	TestFalse("Cube.glb has no skin", LoadSkeletalMeshFromGltf(GetGltfFixturePath(TEXT("Cube.glb")), NoSkin, Error));
	TestTrue("Explained", Error.Contains(TEXT("no skinned mesh")));
	IFileManager::Get().Delete(*Cubic);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGltfAnimationNotifiesTest, "System.MeshUtilities.GltfSkeletal.AnimationNotifies",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGltfAnimationNotifiesTest::RunTest(const FString& Parameters)
{
	// Notifies are authored in an animation's extras (Docs/PLANS/ps2-shipping.md N25): Wave carries Footstep_L at
	// 0.25 s and Footstep_R at 0.75 s, Grip none. Malformed extras fail the import with a clear error.
	FSkeletalMeshImportData Arm;
	if (!LoadArm(*this, Arm))
	{
		return false;
	}
	TArray<FRawAnimSequence> Clips;
	FString Error;
	if (!TestTrue("Loaded",
			LoadAnimSequencesFromGltf(
				GetGltfFixturePath(TEXT("SkinnedArm.glb")), Arm.RefSkeleton, FString(), Clips, Error)) ||
		Clips.Num() != 2)
	{
		AddError(Error);
		return false;
	}
	const TArray<FRawAnimNotify>& Wave = Clips[0].Notifies;
	TestTrue("Wave's two notifies, in time order",
		Wave.Num() == 2 && Wave[0].NotifyName == FName(TEXT("Footstep_L")) &&
			Wave[1].NotifyName == FName(TEXT("Footstep_R")));
	TestTrue("Their times",
		Wave.Num() == 2 && FMath::IsNearlyEqual(Wave[0].Time, 0.25f, 1.0e-5f) &&
			FMath::IsNearlyEqual(Wave[1].Time, 0.75f, 1.0e-5f));
	TestEqual("Grip has none", Clips[1].Notifies.Num(), 0);

	// A notify without a time.
	TArray<uint8> Bytes;
	(void)FFileHelper::LoadFileToArray(Bytes, *GetGltfFixturePath(TEXT("SkinnedArm.glb")));
	const FString Json = TEXT("{\"notifies\":[{\"name\":\"Footstep_L\",\"time\":0.25}");
	const FString Bad = TEXT("{\"notifies\":[{\"name\":\"Footstep_L\",\"frame\":7.5}");
	int32 Found = INDEX_NONE;
	// TCHAR is UTF-8: the strings are the file's bytes.
	for (int32 Index = 0; Index + Json.Len() <= Bytes.Num() && Found == INDEX_NONE; ++Index)
	{
		if (FMemory::Memcmp(Bytes.GetData() + Index, *Json, Json.Len()) == 0)
		{
			Found = Index;
		}
	}
	if (!TestTrue("The fixture's extras", Found != INDEX_NONE))
	{
		return false;
	}
	// Same length, so the chunk sizes stay right.
	check(Bad.Len() == Json.Len());
	FMemory::Memcpy(Bytes.GetData() + Found, *Bad, Bad.Len());
	const FString BadPath =
		FPaths::ConvertRelativePathToFull(FPaths::EngineIntermediateDir() / TEXT("Tests") / TEXT("BadNotify.glb"));
	(void)FFileHelper::SaveArrayToFile(Bytes, *BadPath);
	TestFalse("A notify without a time is rejected",
		LoadAnimSequencesFromGltf(BadPath, Arm.RefSkeleton, FString(), Clips, Error));
	TestTrue("It says why", Error.Contains(TEXT("\"time\"")));
	IFileManager::Get().Delete(*BadPath);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGltfAnimationLoopFlagTest, "System.MeshUtilities.GltfSkeletal.AnimationLoopFlag",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGltfAnimationLoopFlagTest::RunTest(const FString& Parameters)
{
	// The loop flag rides in an animation's extras too (Docs/PLANS/ps2-shipping.md N27; leon_art writes it for every
	// clip): Grip says {"loop": 0} and must not wrap (a montage's clip, a jump's start), Wave has no flag and loops.
	FSkeletalMeshImportData Arm;
	if (!LoadArm(*this, Arm))
	{
		return false;
	}
	TArray<FRawAnimSequence> Clips;
	FString Error;
	if (!TestTrue("Loaded",
			LoadAnimSequencesFromGltf(
				GetGltfFixturePath(TEXT("SkinnedArm.glb")), Arm.RefSkeleton, FString(), Clips, Error)) ||
		Clips.Num() != 2)
	{
		AddError(Error);
		return false;
	}
	TestTrue("Wave loops (no flag)", Clips[0].bLoop);
	TestFalse("Grip does not ({\"loop\": 0})", Clips[1].bLoop);

	// The sequence keeps it: a one-shot holds its last frame and finishes.
	UAnimSequence* Grip = NewObject<UAnimSequence>();
	if (TestTrue("Grip compressed", Grip->SetFromRawAnimSequence(Clips[1])))
	{
		TestFalse("The asset does not loop", Grip->bLoop);
		TestTrue("It finishes at its end", Grip->IsFinished(Grip->SequenceLength));
		TestEqual(
			"It holds its last frame", Grip->GetFrameAtTime(Grip->SequenceLength * 1.5f), float(Grip->NumFrames - 1));
	}

	// A flag that is not 1 or 0 fails the import with a clear error.
	TArray<uint8> Bytes;
	(void)FFileHelper::LoadFileToArray(Bytes, *GetGltfFixturePath(TEXT("SkinnedArm.glb")));
	const FString Json = TEXT("{\"loop\":0}");
	const FString Bad = TEXT("{\"loop\":2}");
	int32 Found = INDEX_NONE;
	for (int32 Index = 0; Index + Json.Len() <= Bytes.Num() && Found == INDEX_NONE; ++Index)
	{
		if (FMemory::Memcmp(Bytes.GetData() + Index, *Json, Json.Len()) == 0)
		{
			Found = Index;
		}
	}
	if (!TestTrue("The fixture's extras", Found != INDEX_NONE))
	{
		return false;
	}
	FMemory::Memcpy(Bytes.GetData() + Found, *Bad, Bad.Len());
	const FString BadPath =
		FPaths::ConvertRelativePathToFull(FPaths::EngineIntermediateDir() / TEXT("Tests") / TEXT("BadLoop.glb"));
	(void)FFileHelper::SaveArrayToFile(Bytes, *BadPath);
	TestFalse("A loop of 2 is rejected", LoadAnimSequencesFromGltf(BadPath, Arm.RefSkeleton, FString(), Clips, Error));
	TestTrue("It says why", Error.Contains(TEXT("\"loop\"")));
	IFileManager::Get().Delete(*BadPath);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
