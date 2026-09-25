#include "ThirdPersonGameMode.h"

#include "CoreGlobals.h"
#include "GenericPlatform/GenericWindow.h"
#include "GenericPlatform/IInputInterface.h"
#include "Misc/CString.h"
#include "PS2RHI.h"
#include "Stats/StatsOverlay.h"
#include "ThirdPerson.h"

namespace
{
	// On-screen debug message slots below the engine stats.
	constexpr int32 BoxesMessageKey = 0;
	constexpr int32 TrianglesMessageKey = 1;

	void PrintBanner()
	{
		UE_LOG(LogThirdPerson, Display, TEXT("======= Leon ThirdPerson ======="));
		UE_LOG(LogThirdPerson, Display, TEXT("  Camera follows character + orbit"));
		UE_LOG(LogThirdPerson, Display, TEXT("  Large grounded primitive level"));
		UE_LOG(LogThirdPerson, Display, TEXT("  Left stick    move (cam-relative)"));
		UE_LOG(LogThirdPerson, Display, TEXT("  Right stick   camera orbit"));
		UE_LOG(LogThirdPerson, Display, TEXT("  Cross         jump"));
		UE_LOG(LogThirdPerson, Display, TEXT("  Start         quit | Select  debug HUD"));
		UE_LOG(LogThirdPerson, Display, TEXT("Draw3D stats: HUD + PCSX2 console every 30 frames"));
	}
} // namespace

FThirdPersonGameMode::FThirdPersonGameMode(FGenericWindow& InWindow, IInputInterface* InInputInterface)
	: Window(InWindow)
	, InputInterface(InInputInterface)
{
}

FThirdPersonGameMode::~FThirdPersonGameMode()
{
	FStatsOverlay::ClearOnScreenDebugMessage(BoxesMessageKey);
	FStatsOverlay::ClearOnScreenDebugMessage(TrianglesMessageKey);
}

void FThirdPersonGameMode::StartPlay()
{
	for (int32 Index = 0; Index < 2; ++Index)
	{
		FPS2RHI::ClearColor(0.08f, 0.10f, 0.14f);
		Window.SwapBuffers();
	}

	PrintBanner();

	GridTexture = FPS2Texture::CreateGrid(64);
	CheckerTexture = FPS2Texture::CreateChecker(64);

	GroundMaterial.BaseColorR = 0.72f;
	GroundMaterial.BaseColorG = 0.76f;
	GroundMaterial.BaseColorB = 0.70f;
	GroundMaterial.BaseColorMap = GridTexture.Valid() ? &GridTexture : nullptr;

	PlatformMaterial.BaseColorR = 0.52f;
	PlatformMaterial.BaseColorG = 0.56f;
	PlatformMaterial.BaseColorB = 0.62f;
	PlatformMaterial.BaseColorMap = CheckerTexture.Valid() ? &CheckerTexture : nullptr;

	CrateMaterial.BaseColorR = 0.85f;
	CrateMaterial.BaseColorG = 0.55f;
	CrateMaterial.BaseColorB = 0.25f;
	CrateMaterial.BaseColorMap = CheckerTexture.Valid() ? &CheckerTexture : nullptr;

	CharacterMaterial.BaseColorR = 0.25f;
	CharacterMaterial.BaseColorG = 0.55f;
	CharacterMaterial.BaseColorB = 0.95f;

	Level.Build(&GroundMaterial, &PlatformMaterial, &CrateMaterial);
	Character.LoadConfig();
	Character.SpawnAt(Level);
	CameraBoom.ResetTo(Character.LocationX, Character.LocationY, Character.LocationZ);

	Sun.Intensity = 1.05f;
	Sun.Yaw256 = 20;
	Sun.Pitch256 = 78;
	FPS2RHI::SetDirectionalLight(Sun);
	FPS2RHI::SetAmbientLightColor(0.16f, 0.18f, 0.24f);

	CameraBoom.UpdateViewTarget(Level);
}

bool FThirdPersonGameMode::Tick(float DeltaTime)
{
	// Fixed-step gameplay tuned per frame (60 Hz vsync), as in the original demo.
	(void)DeltaTime;

	if (IsGamepadKeyDown(EKeys::Gamepad_Special_Right))
	{
		UE_LOG(LogThirdPerson, Display, TEXT("Quit after %u frames"), FrameNumber);
		RequestEngineExit("ThirdPerson: Start pressed");
		return false;
	}

	CameraBoom.AddOrbitInput(GetGamepadAnalog(EKeys::Gamepad_RightX), GetGamepadAnalog(EKeys::Gamepad_RightY));

	const bool bJumpDown = IsGamepadKeyDown(EKeys::Gamepad_FaceButton_Bottom);
	const bool bJumpPressed = bJumpDown && !bPrevJump;
	bPrevJump = bJumpDown;
	Character.Move(GetGamepadAnalog(EKeys::Gamepad_LeftY), GetGamepadAnalog(EKeys::Gamepad_LeftX),
		CameraBoom.GetWrappedYaw256(), bJumpPressed, Level);

	CameraBoom.Follow(Character.LocationX, Character.LocationY, Character.LocationZ);

	Sun.Yaw256 = (20u + (FrameNumber / 3u)) & 255u;
	FPS2RHI::SetDirectionalLight(Sun);
	CameraBoom.UpdateViewTarget(Level);

	FPS2RHI::ClearColor(0.12f, 0.16f, 0.22f);
	Level.Draw();
	Character.Draw(CharacterMaterial);

	UpdateStatsMessages();
	++FrameNumber;
	return true;
}

bool FThirdPersonGameMode::IsGamepadKeyDown(EKeys Key) const
{
	return InputInterface != nullptr && InputInterface->IsGamepadKeyDown(Key);
}

float FThirdPersonGameMode::GetGamepadAnalog(EKeys Axis) const
{
	return InputInterface != nullptr ? InputInterface->GetGamepadAnalog(Axis) : 0.0f;
}

/** Draw3D counters for the engine stats panel: boxes drawn / submitted, GS triangles, clipped. */
void FThirdPersonGameMode::UpdateStatsMessages()
{
	FPS2Draw3DStats DrawStats{};
	FPS2RHI::GetDraw3DStats(DrawStats);

	char Boxes[32];
	char Triangles[32];
	FCString::Snprintf(Boxes, sizeof(Boxes), "BOXES %u/%u", DrawStats.Boxes - DrawStats.CulledBoxes, DrawStats.Boxes);
	FCString::Snprintf(Triangles, sizeof(Triangles), "TRIS %u CLIP %u", DrawStats.Emitted, DrawStats.Clipped);
	FStatsOverlay::AddOnScreenDebugMessage(BoxesMessageKey, Boxes);
	FStatsOverlay::AddOnScreenDebugMessage(TrianglesMessageKey, Triangles);

	if ((FrameNumber % 30u) == 0u)
	{
		FPS2RHI::PrintDraw3DStats(DrawStats);
	}
}
