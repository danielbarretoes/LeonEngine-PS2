#include "Camera/CameraComponent.h"
#include "CoreMinimal.h"
#include "Frustum.h"
#include "GLClipSpace.h"
#include "GameFramework/DefaultCameraActor.h"
#include "GameFramework/Input.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Misc/AutomationTest.h"
#include "Physics/PhysScene.h"
#include "SceneRenderer.h"
#include "ShadowMap.h"
#include "Tests/LegacyGolden.h"
#include "Tests/ScopedTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// View, camera and rendering-math goldens, recorded in the legacy world before P7. Clip-space NDC is unitless and
// must come out identical after P7: only the inputs go through the LegacyGolden adapters.

namespace
{

	constexpr float GoldenViewPositionTolerance = 1.0e-3f;
	constexpr float GoldenViewDirectionTolerance = 1.0e-4f;
	constexpr float GoldenViewNdcTolerance = 1.0e-4f;

	/** Clip-space NDC (x / w, y / w, z / w) of a world point through a view-projection, as the renderer applies it. */
	FVector GoldenNdc(const FMatrix& ViewProjection, const FVector& WorldPoint)
	{
		const FVector4 Clip = ViewProjection.TransformFVector4(FVector4(WorldPoint, 1.0f));
		return FVector(Clip.X / Clip.W, Clip.Y / Clip.W, Clip.Z / Clip.W);
	}

	/** The 16 fixed points of the NDC goldens, in the engine world (legacy x in +/-1.5, y in {0.25, 1.25}, z +/-1). */
	TArray<FVector> GoldenViewPoints()
	{
		TArray<FVector> Points;
		for (int32 Ix = 0; Ix < 4; ++Ix)
		{
			for (int32 Iy = 0; Iy < 2; ++Iy)
			{
				for (int32 Iz = 0; Iz < 2; ++Iz)
				{
					const FVector Legacy(-1.5f + static_cast<float>(Ix), 0.25f + static_cast<float>(Iy),
						-1.0f + (2.0f * static_cast<float>(Iz)));
					Points.Add(LegacyGolden::ToWorldPosition(Legacy));
				}
			}
		}
		return Points;
	}

	/** A 60 degree, 16:9 perspective camera with near 0.1 m and far 100 m. */
	void SetGoldenPerspective(UCameraComponent& Camera)
	{
		Camera.SetPerspective(
			60.0f, 16.0f / 9.0f, LegacyGolden::ToWorldLength(0.1f), LegacyGolden::ToWorldLength(100.0f));
	}

	/** The camera's projection in GL clip space, as the renderer draws with it. */
	FMatrix GoldenProjectionGL(const UCameraComponent& Camera)
	{
		return ToGLClipSpace(Camera.ProjectionMatrix());
	}

	/** The renderer's view-projection of a camera (UE view, then the projection in GL clip space). */
	FMatrix GoldenViewProjection(const UCameraComponent& Camera)
	{
		return Camera.ViewMatrix() * GoldenProjectionGL(Camera);
	}

	/** Appends the NDC of every point through a view-projection. */
	void AppendGoldenNdc(TArray<FVector>& OutNdc, const FMatrix& ViewProjection, const TArray<FVector>& WorldPoints)
	{
		for (const FVector& Point : WorldPoints)
		{
			OutNdc.Add(GoldenNdc(ViewProjection, Point));
		}
	}

	/** An orbit camera around a legacy target, with legacy orbit angles and distance. */
	void SetGoldenOrbit(UCameraComponent& Camera, const FVector& LegacyTarget, float Yaw, float Pitch, float Distance)
	{
		SetGoldenPerspective(Camera);
		Camera.SetMode(ECameraMode::Orbit);
		Camera.SetTarget(LegacyGolden::ToWorldPosition(LegacyTarget));
		Camera.SetViewRotation(FLegacyCoordinateConversion::ConvertOrbitViewRotation(Yaw, Pitch));
		Camera.SetDistance(LegacyGolden::ToWorldLength(Distance));
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoldenSpringArmTest, "System.Engine.Golden.SpringArm",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGoldenSpringArmTest::RunTest(const FString& Parameters)
{
	// Spring arm without lag for four boom orientations, first in open space, then with a box 2 m along the boom:
	// the camera target, eye and probed arm length. The boom angles are the yaw (from +X toward the second horizontal
	// axis) and the elevation of the target-to-eye arm; legacy (X, Z) is the world's (X, Y) in the same order, so the
	// legacy angles are the world ones. The arm follows the pawn's control rotation, which looks back along the boom.
	constexpr float Orientations[4][2] = {{0.0f, 15.0f}, {90.0f, 30.0f}, {200.0f, -10.0f}, {315.0f, 50.0f}};
	const FVector ActorLocation = LegacyGolden::ToWorldPosition(FVector(1.0f, 0.0f, -2.0f));

	TArray<FVector> Targets;
	TArray<FVector> Eyes;
	TArray<float> Lengths;
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		const bool bWithObstacle = Pass == 1;
		for (const auto& Orientation : Orientations)
		{
			FScopedTestWorld TestWorld;
			ADefaultCameraActor& Pawn = *TestWorld->SpawnActor<ADefaultCameraActor>();
			APlayerController Controller;
			Controller.Possess(&Pawn);
			Controller.SetControlRotation(FRotator(-Orientation[1], Orientation[0] + 180.0f, 0.0f));
			USpringArmComponent& Arm = *NewObject<USpringArmComponent>(&Pawn);
			Arm.bUsePawnControlRotation = true;
			Arm.bDoCollisionTest = true;
			Arm.bEnableCameraLag = false;
			Arm.bEnableCameraRotationLag = false;
			Arm.ArmLengthLagSpeed = 1000.0f;
			Arm.TargetArmLength = LegacyGolden::ToWorldLength(4.0f);
			Arm.ArmLengthMin = LegacyGolden::ToWorldLength(0.5f);
			Arm.TargetOffset = FVector(0.0f, 0.0f, LegacyGolden::ToWorldLength(1.0f));
			Arm.SocketOffset = FVector(0.0f, LegacyGolden::ToWorldLength(0.3f), 0.0f);
			Arm.ProbeSize = LegacyGolden::ToWorldLength(0.15f);
			Arm.CollisionProbeOffset = LegacyGolden::ToWorldLength(0.05f);
			Arm.SnapLagState(ActorLocation);

			FPhysScene Scene;
			if (bWithObstacle)
			{
				const FVector BoomDirection = FRotator(Orientation[1], Orientation[0], 0.0f).Vector();
				const int32 Id = Scene.AddBody({0, EBodyType::Static, 1.0f, true});
				Scene.GetBodies()[Id].Position =
					Arm.GetArmOrigin(ActorLocation) + (BoomDirection * LegacyGolden::ToWorldLength(2.0f));
				Scene.GetBodies()[Id].HalfExtents = LegacyGolden::ToWorldExtent(FVector(0.4f, 0.4f, 0.4f));
			}

			UCameraComponent& Camera = *NewObject<UCameraComponent>();
			Arm.ApplyToCamera(Camera, ActorLocation, 1.0f / 60.0f, &Scene);
			Targets.Add(Camera.GetTarget());
			Eyes.Add(Camera.GetCameraLocation());
			Lengths.Add(Camera.GetDistance());
		}
	}

	static const FVector ExpectedTargets[8] = {FVector(1.0f, 1.0f, -2.29999995f), FVector(1.29999995f, 1.0f, -2.0f),
		FVector(0.897393942f, 1.0f, -1.7180922f), FVector(0.787867904f, 1.0f, -2.21213198f),
		FVector(1.0f, 1.0f, -2.29999995f), FVector(1.29999995f, 1.0f, -2.0f), FVector(0.897393942f, 1.0f, -1.7180922f),
		FVector(0.787867904f, 1.0f, -2.21213198f)};
	static const FVector ExpectedEyes[8] = {FVector(4.86370325f, 2.03527617f, -2.29999995f),
		FVector(1.29999983f, 3.0f, 1.46410155f), FVector(-2.80427217f, 0.305407286f, -3.06538868f),
		FVector(2.60594559f, 4.06417751f, -4.03021049f), FVector(2.33355522f, 1.35732508f, -2.29999995f),
		FVector(1.29999995f, 1.65745735f, -0.86125052f), FVector(-0.357168436f, 0.764589787f, -2.17471552f),
		FVector(1.34784758f, 1.94378662f, -2.77211189f)};
	static const float ExpectedLengths[8] = {4.0f, 4.0f, 4.0f, 4.0f, 1.38059807f, 1.3149147f, 1.35567319f, 1.23202586f};
	LegacyGolden::CheckPositions(*this, "Targets", Targets, ExpectedTargets, 8, GoldenViewPositionTolerance);
	LegacyGolden::CheckPositions(*this, "Eyes", Eyes, ExpectedEyes, 8, GoldenViewPositionTolerance);
	LegacyGolden::CheckScalars(
		*this, "Lengths", Lengths, ExpectedLengths, 8, GoldenViewPositionTolerance, LegacyGolden::EUnit::Length);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoldenYawRelativeMoveTest, "System.Engine.Golden.YawRelativeMove",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGoldenYawRelativeMoveTest::RunTest(const FString& Parameters)
{
	// The ground-plane move direction of forward and of forward-right input for six legacy orbit camera yaws (the view
	// yaw of an orbit camera is its legacy yaw + 180).
	constexpr float Yaws[6] = {0.0f, 45.0f, 90.0f, 180.0f, -30.0f, 270.0f};
	TArray<FVector> Moves;
	for (const float Yaw : Yaws)
	{
		const FRotator ViewRotation = FLegacyCoordinateConversion::ConvertOrbitViewRotation(Yaw, 0.0f);
		Moves.Add(YawRelativeMove(ViewRotation, FVector2D(1.0f, 0.0f)));
		Moves.Add(YawRelativeMove(ViewRotation, FVector2D(1.0f, 1.0f)));
	}

	static const FVector ExpectedMoves[12] = {FVector(-1.0f, 0.0f, -0.0f), FVector(-0.707106769f, 0.0f, -0.707106769f),
		FVector(-0.707106829f, 0.0f, -0.707106829f), FVector(0.0f, 0.0f, -0.99999994f),
		FVector(4.37113883e-08f, 0.0f, -1.0f), FVector(0.707106769f, 0.0f, -0.707106709f),
		FVector(1.0f, 0.0f, 8.74227766e-08f), FVector(0.707106709f, 0.0f, 0.707106829f),
		FVector(-0.866025388f, 0.0f, 0.5f), FVector(-0.965925753f, 0.0f, -0.258819014f),
		FVector(-1.19248806e-08f, 0.0f, 1.0f), FVector(-0.707106769f, 0.0f, 0.707106769f)};
	LegacyGolden::CheckDirections(*this, "Moves", Moves, ExpectedMoves, 12, GoldenViewDirectionTolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoldenOrbitCameraNdcTest, "System.Engine.Golden.OrbitCameraNdc",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGoldenOrbitCameraNdcTest::RunTest(const FString& Parameters)
{
	// Two orbit cameras: the eye, and the NDC of the 16 fixed points through projection * view.
	const TArray<FVector> Points = GoldenViewPoints();
	TArray<FVector> Eyes;
	TArray<FVector> Ndc;

	UCameraComponent& First = *NewObject<UCameraComponent>();
	SetGoldenOrbit(First, FVector(0.0f, 0.5f, 0.0f), 30.0f, 20.0f, 6.0f);
	Eyes.Add(First.GetCameraLocation());
	AppendGoldenNdc(Ndc, GoldenViewProjection(First), Points);

	UCameraComponent& Second = *NewObject<UCameraComponent>();
	SetGoldenOrbit(Second, FVector(0.5f, 1.0f, -0.5f), -120.0f, 45.0f, 9.0f);
	Eyes.Add(Second.GetCameraLocation());
	AppendGoldenNdc(Ndc, GoldenViewProjection(Second), Points);

	static const FVector ExpectedEyes[2] = {
		FVector(4.88278627f, 2.55212069f, 2.81907797f), FVector(-2.68198085f, 7.36396074f, -6.01135159f)};
	static const FVector ExpectedNdc[32] = {FVector(0.0145371174f, 0.0847274065f, 0.976256192f),
		FVector(-0.230306745f, 0.00971982069f, 0.972717404f), FVector(0.0152059328f, 0.30756408f, 0.975071788f),
		FVector(-0.242435709f, 0.260849446f, 0.971175075f), FVector(0.0862049684f, 0.0209435318f, 0.973246932f),
		FVector(-0.180541128f, -0.0741515681f, 0.968760252f), FVector(0.0906585678f, 0.26787734f, 0.971761346f),
		FVector(-0.191411361f, 0.207905248f, 0.966758847f), FVector(0.176844493f, -0.0597249568f, 0.969440997f),
		FVector(-0.115225144f, -0.184230477f, 0.96356672f), FVector(0.187261268f, 0.217066109f, 0.967522979f),
		FVector(-0.123322822f, 0.137254775f, 0.960865617f), FVector(0.295137972f, -0.165005282f, 0.964473724f),
		FVector(-0.0257205553f, -0.33507511f, 0.956449926f), FVector(0.315356374f, 0.149690375f, 0.961902857f),
		FVector(-0.0278910641f, 0.0382322706f, 0.952605784f), FVector(0.16953437f, -0.313915879f, 0.978496134f),
		FVector(0.248230696f, -0.0566952452f, 0.981451273f), FVector(0.184883922f, -0.185518712f, 0.976367891f),
		FVector(0.26765871f, 0.0744279325f, 0.979842901f), FVector(0.0676596016f, -0.232370153f, 0.97943306f),
		FVector(0.155959114f, 0.00594927371f, 0.982171059f), FVector(0.0735201538f, -0.102470428f, 0.977478147f),
		FVector(0.1677057f, 0.136852741f, 0.980677366f), FVector(-0.0264056437f, -0.157075599f, 0.980298102f),
		FVector(0.0699317753f, 0.0643544346f, 0.982841969f), FVector(-0.02859791f, -0.0263170358f, 0.978496134f),
		FVector(0.0750077739f, 0.194746435f, 0.981451273f), FVector(-0.113526188f, -0.0873399228f, 0.981099188f),
		FVector(-0.0104643591f, 0.118936531f, 0.983469069f), FVector(-0.122575767f, 0.0437659137f, 0.97943294f),
		FVector(-0.0111973127f, 0.248585075f, 0.982170999f)};
	LegacyGolden::CheckPositions(*this, "Eyes", Eyes, ExpectedEyes, 2, GoldenViewPositionTolerance);
	LegacyGolden::CheckUnitlessVectors(*this, "Ndc", Ndc, ExpectedNdc, 32, GoldenViewNdcTolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoldenFreeLookCameraNdcTest, "System.Engine.Golden.FreeLookCameraNdc",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGoldenFreeLookCameraNdcTest::RunTest(const FString& Parameters)
{
	// Two free-look cameras facing the fixed points: the forward vector, and the NDC of the 16 points.
	const TArray<FVector> Points = GoldenViewPoints();
	constexpr float Settings[2][5] = {{6.0f, 2.0f, 0.5f, 180.0f, -15.0f}, {-1.0f, 3.0f, 7.0f, -80.0f, -20.0f}};
	TArray<FVector> Forwards;
	TArray<FVector> Ndc;
	for (const auto& Setting : Settings)
	{
		UCameraComponent& Camera = *NewObject<UCameraComponent>();
		SetGoldenPerspective(Camera);
		Camera.SetMode(ECameraMode::FreeLook);
		Camera.SetEyeLocation(LegacyGolden::ToWorldPosition(FVector(Setting[0], Setting[1], Setting[2])));
		Camera.SetViewRotation(FLegacyCoordinateConversion::ConvertFreeLookRotation(Setting[3], Setting[4]));
		Forwards.Add(Camera.ForwardVector());
		AppendGoldenNdc(Ndc, GoldenViewProjection(Camera), Points);
	}

	static const FVector ExpectedForwards[2] = {
		FVector(-0.965925932f, -0.258819073f, -8.44439256e-08f), FVector(0.16317597f, -0.342020154f, -0.925416648f)};
	static const FVector ExpectedNdc[32] = {FVector(0.189859107f, 0.0564283989f, 0.975993156f),
		FVector(-0.0632864907f, 0.0564283989f, 0.975993156f), FVector(0.196465105f, 0.283305317f, 0.97508812f),
		FVector(-0.0654884949f, 0.283305347f, 0.975088179f), FVector(0.217102855f, -0.00207042065f, 0.972261012f),
		FVector(-0.0723677427f, -0.00207043835f, 0.972261012f), FVector(0.225784078f, 0.256324708f, 0.97107178f),
		FVector(-0.0752614886f, 0.256324708f, 0.97107172f), FVector(0.25347513f, -0.0801704228f, 0.967278302f),
		FVector(-0.0844918266f, -0.0801704377f, 0.967278302f), FVector(0.265388638f, 0.219878748f, 0.965646327f),
		FVector(-0.088463001f, 0.219878748f, 0.965646267f), FVector(0.304487348f, -0.189705893f, 0.960290253f),
		FVector(-0.101495914f, -0.189705908f, 0.960290253f), FVector(0.32184276f, 0.167927116f, 0.957912624f),
		FVector(-0.107281059f, 0.167927131f, 0.957912683f), FVector(-0.221874341f, 0.0169261303f, 0.977771401f),
		FVector(-0.23314932f, -0.160172999f, 0.970776796f), FVector(-0.23145552f, 0.223154247f, 0.976725161f),
		FVector(-0.24628754f, 0.0989631712f, 0.969017208f), FVector(-0.10369923f, 0.0288075507f, 0.978240728f),
		FVector(-0.0814267844f, -0.140551418f, 0.971551776f), FVector(-0.108086862f, 0.231375381f, 0.977235377f),
		FVector(-0.0858951434f, 0.112877078f, 0.96988076f), FVector(0.00998546649f, 0.0402375013f, 0.978692174f),
		FVector(0.0629469529f, -0.121880211f, 0.972289145f), FVector(0.0103996033f, 0.239271179f, 0.977725446f),
		FVector(0.0663130879f, 0.12608102f, 0.970700204f), FVector(0.119430915f, 0.0512412302f, 0.979126751f),
		FVector(0.200493157f, -0.10409198f, 0.972991705f), FVector(0.124288075f, 0.246860549f, 0.978196442f),
		FVector(0.210947946f, 0.138627976f, 0.971478939f)};
	LegacyGolden::CheckDirections(*this, "Forwards", Forwards, ExpectedForwards, 2, GoldenViewDirectionTolerance);
	LegacyGolden::CheckUnitlessVectors(*this, "Ndc", Ndc, ExpectedNdc, 32, GoldenViewNdcTolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoldenShadowLightSpaceTest, "System.Engine.Golden.ShadowLightSpace",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGoldenShadowLightSpaceTest::RunTest(const FString& Parameters)
{
	// The shadow map's fitted light matrix for two light directions: NDC of the 8 corners of the caster box.
	const FVector LegacyMin(-3.0f, 0.0f, -2.0f);
	const FVector LegacyMax(4.0f, 2.5f, 3.0f);
	const FVector LightDirections[2] = {FVector(0.35f, -1.0f, -0.45f), FVector(-0.6f, -0.5f, 0.2f)};

	TArray<FVector> Ndc;
	for (const FVector& LightDirection : LightDirections)
	{
		const FMatrix LightSpace = FShadowMap::FitLightSpaceMatrix(LegacyGolden::ToWorldDirection(LightDirection),
			LegacyGolden::ToWorldPosition(LegacyMin), LegacyGolden::ToWorldPosition(LegacyMax),
			LegacyGolden::ToWorldLength(0.5f));
		for (int32 Corner = 0; Corner < 8; ++Corner)
		{
			const FVector Legacy((Corner & 1) != 0 ? LegacyMax.X : LegacyMin.X,
				(Corner & 2) != 0 ? LegacyMax.Y : LegacyMin.Y, (Corner & 4) != 0 ? LegacyMax.Z : LegacyMin.Z);
			Ndc.Add(GoldenNdc(LightSpace, LegacyGolden::ToWorldPosition(Legacy)));
		}
	}

	static const FVector ExpectedNdc[16] = {FVector(-0.895780861f, -0.164134532f, 0.275413156f),
		FVector(0.255937338f, 0.630195498f, 0.862163067f), FVector(-0.895780861f, 0.0992912278f, -0.323311269f),
		FVector(0.255937338f, 0.893621266f, 0.263438642f), FVector(-0.255937368f, -0.893621266f, -0.263438821f),
		FVector(0.895780861f, -0.0992912427f, 0.32331112f), FVector(-0.255937368f, -0.630195498f, -0.862163186f),
		FVector(0.895780861f, 0.164134562f, -0.275413275f), FVector(0.874324679f, 0.145988911f, 0.613266468f),
		FVector(0.317936242f, -0.875933409f, -0.544360042f), FVector(0.874324679f, 0.632618546f, 0.268734783f),
		FVector(0.317936242f, -0.389303803f, -0.888891697f), FVector(-0.317936182f, 0.389303774f, 0.888891876f),
		FVector(-0.87432462f, -0.632618546f, -0.268734634f), FVector(-0.317936182f, 0.875933409f, 0.544360161f),
		FVector(-0.87432462f, -0.145988941f, -0.613266349f)};
	LegacyGolden::CheckUnitlessVectors(*this, "Ndc", Ndc, ExpectedNdc, 16, GoldenViewNdcTolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoldenPlanarReflectionTest, "System.Engine.Golden.PlanarReflection",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGoldenPlanarReflectionTest::RunTest(const FString& Parameters)
{
	// The planar mirror about the floor at 0.25 m: six mirrored points, and their NDC through the reflected
	// view-projection of an orbit camera (as the renderer composes it).
	const FMatrix Reflect = FSceneRenderer::MakeReflectMatrix(LegacyGolden::ToWorldLength(0.25f));
	const FVector LegacyPoints[6] = {FVector(0.0f, 0.25f, 0.0f), FVector(1.0f, 1.0f, 0.0f), FVector(-2.0f, 0.5f, 1.5f),
		FVector(0.5f, 3.0f, -1.0f), FVector(1.5f, -0.5f, 2.0f), FVector(-1.0f, 2.25f, -2.0f)};

	UCameraComponent& Camera = *NewObject<UCameraComponent>();
	SetGoldenOrbit(Camera, FVector(0.0f, 0.5f, 0.0f), 30.0f, 20.0f, 6.0f);
	const FMatrix ReflectionViewProjection = Reflect * Camera.ViewMatrix() * GoldenProjectionGL(Camera);

	TArray<FVector> Mirrored;
	TArray<FVector> Ndc;
	for (const FVector& Legacy : LegacyPoints)
	{
		const FVector Point = LegacyGolden::ToWorldPosition(Legacy);
		Mirrored.Add(FVector(Reflect.TransformPosition(Point)));
		Ndc.Add(GoldenNdc(ReflectionViewProjection, Point));
	}

	static const FVector ExpectedMirrored[6] = {FVector(0.0f, 0.25f, 0.0f), FVector(1.0f, -0.5f, 0.0f),
		FVector(-2.0f, 0.0f, 1.5f), FVector(0.5f, -2.5f, -1.0f), FVector(1.5f, 1.0f, 2.0f),
		FVector(-1.0f, -1.75f, -2.0f)};
	static const FVector ExpectedNdc[6] = {FVector(3.81703913e-08f, -0.0668636486f, 0.969104171f),
		FVector(0.08811865f, -0.38721776f, 0.965787768f), FVector(-0.31575346f, -0.0327093452f, 0.973780334f),
		FVector(0.153381109f, -0.683184922f, 0.973761082f), FVector(-0.260805368f, -0.149414957f, 0.94743073f),
		FVector(0.140837252f, -0.299971044f, 0.978512704f)};
	LegacyGolden::CheckPositions(*this, "Mirrored", Mirrored, ExpectedMirrored, 6, GoldenViewPositionTolerance);
	LegacyGolden::CheckUnitlessVectors(*this, "Ndc", Ndc, ExpectedNdc, 6, GoldenViewNdcTolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoldenFrustumCullingTest, "System.Engine.Golden.FrustumCulling",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGoldenFrustumCullingTest::RunTest(const FString& Parameters)
{
	// Frustum culling of 20 boxes around an orbit camera's target, near and far, in view and out of it.
	UCameraComponent& Camera = *NewObject<UCameraComponent>();
	SetGoldenOrbit(Camera, FVector::ZeroVector, 30.0f, 20.0f, 8.0f);
	FFrustum Frustum;
	Frustum.ExtractFromViewProjection(GoldenViewProjection(Camera));

	constexpr float Radii[4] = {2.0f, 6.0f, 14.0f, 120.0f};
	TArray<bool> Visible;
	for (int32 Index = 0; Index < 20; ++Index)
	{
		const float Angle = FMath::DegreesToRadians(18.0f * static_cast<float>(Index));
		const float Radius = Radii[Index % 4];
		const FVector LegacyCenter(
			Radius * FMath::Cos(Angle), static_cast<float>(Index % 3) - 1.0f, Radius * FMath::Sin(Angle));
		const FVector Center = LegacyGolden::ToWorldPosition(LegacyCenter);
		const FVector Extent = LegacyGolden::ToWorldExtent(FVector(0.5f + (0.5f * static_cast<float>(Index % 2))));
		Visible.Add(Frustum.IntersectsAabb(FBox(Center - Extent, Center + Extent)));
	}

	static const bool ExpectedVisible[20] = {true, true, false, false, true, true, false, false, true, true, true,
		false, true, true, true, false, true, true, false, false};
	LegacyGolden::CheckBools(*this, "Visible", Visible, ExpectedVisible, 20);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
