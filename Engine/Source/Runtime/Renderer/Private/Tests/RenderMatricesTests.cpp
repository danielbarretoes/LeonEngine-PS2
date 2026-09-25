#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "RenderMatrices.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRenderMatricesNormalMatrixTest, "System.Renderer.RenderMatrices.NormalMatrix",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FRenderMatricesNormalMatrixTest::RunTest(const FString& Parameters)
{
	// glm's transpose(inverse(mat3(Model))) of translate(1, -2, 3.5) * rotate X 30 * rotate Y -45 * rotate Z 60 *
	// scale(2, 0.5, 1.5), printed by glm 1.0.1 in mat3 memory order.
	const FQuat RotX(FVector(1.0f, 0.0f, 0.0f), FMath::DegreesToRadians(30.0f));
	const FQuat RotY(FVector(0.0f, 1.0f, 0.0f), FMath::DegreesToRadians(-45.0f));
	const FQuat RotZ(FVector(0.0f, 0.0f, 1.0f), FMath::DegreesToRadians(60.0f));
	const FMatrix Model = FScaleMatrix(FVector(2.0f, 0.5f, 1.5f)) * FQuatRotationMatrix(RotZ) *
		FQuatRotationMatrix(RotY) * FQuatRotationMatrix(RotX) * FTranslationMatrix(FVector(1.0f, -2.0f, 3.5f));
	const float Expected[9] = {0.176776692f, 0.286611646f, 0.369599462f, -1.22474504f, 1.47839773f, -0.560660243f,
		-0.471404523f, -0.235702276f, 0.408248305f};

	float Normal[9];
	GetNormalMatrix3x3(Model, Normal);
	for (int32 Index = 0; Index < 9; ++Index)
	{
		TestEqual(*FString::Printf("Element %d", Index), Normal[Index], Expected[Index], 1.0e-6f);
	}

	// A rotation is its own normal matrix: mat3 column C is the image of axis C (FMatrix row C).
	const FMatrix Rotation = FQuatRotationMatrix(RotX * RotY);
	GetNormalMatrix3x3(Rotation, Normal);
	for (int32 Index = 0; Index < 9; ++Index)
	{
		TestEqual(
			*FString::Printf("Rotation element %d", Index), Normal[Index], Rotation.M[Index / 3][Index % 3], 1.0e-6f);
	}

	// A zero-scale model gives the identity.
	GetNormalMatrix3x3(FScaleMatrix(FVector::ZeroVector), Normal);
	const float Identity[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
	for (int32 Index = 0; Index < 9; ++Index)
	{
		TestEqual(*FString::Printf("Zero scale element %d", Index), Normal[Index], Identity[Index]);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRenderMatricesPixelSpaceProjectionTest,
	"System.Renderer.RenderMatrices.PixelSpaceProjection",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FRenderMatricesPixelSpaceProjectionTest::RunTest(const FString& Parameters)
{
	// Pixel corners map to the GL clip square with y down: top-left (-1, 1), bottom-right (1, -1).
	const FMatrix Projection = MakePixelSpaceProjection(1280.0f, 720.0f);
	TestTrue("Top-left",
		FVector(Projection.TransformPosition(FVector(0.0f, 0.0f, 0.0f))).Equals(FVector(-1.0f, 1.0f, 0.0f)));
	TestTrue("Bottom-right",
		FVector(Projection.TransformPosition(FVector(1280.0f, 720.0f, 0.0f))).Equals(FVector(1.0f, -1.0f, 0.0f)));
	TestTrue(
		"Centre", FVector(Projection.TransformPosition(FVector(640.0f, 360.0f, 0.0f))).Equals(FVector::ZeroVector));

	// glm::ortho(0, 1280, 720, 0, -1, 1) in x and y.
	TestEqual("x scale", Projection.M[0][0], 0.00156250002f);
	TestEqual("y scale", Projection.M[1][1], -0.00277777785f);
	TestEqual("x offset", Projection.M[3][0], -1.0f);
	TestEqual("y offset", Projection.M[3][1], 1.0f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
