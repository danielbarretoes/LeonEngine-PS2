#include "CanvasTypes.h"
#include "FramePacer.h"
#include "GS/GSSceneRenderer.h"
#include "GSEmulator/GSOpenGLEmulator.h"
#include "GSEmulator/PS2TexturePreview.h"
#include "Modules/ModuleManager.h"
#include "RendererInterface.h"
#include "RendererLog.h"
#include "RendererSettings.h"
#include "ScenePrivate.h"
#include "SceneView.h"

DEFINE_LOG_CATEGORY(LogRenderer);

namespace
{

	/**
	 * The Renderer module on the desktop (UE: FRendererModule): the GS scene renderer records the view family and the
	 * canvas as GS lists, as on the PS2, and the OpenGL emulation of the GS draws them into its 640 x 448 frame, shown
	 * in the window at EndDrawingViewport (Docs/PLANS/ps2-gs-parity.md P4 and P5) at the TV's aspect ratio and the
	 * PS2's frame rate (Docs/PLANS/ps2-preview.md V1).
	 */
	class FRendererModule final : public IRendererModule
	{
	public:
		bool InitRenderer(const FString& ShaderDirectory) override
		{
			if (bInitialized)
			{
				return true;
			}
			if (!Emulator.Initialize(ShaderDirectory))
			{
				return false;
			}
			uint32 FirstBlock = 0;
			uint32 NumBlocks = 0;
			FGSOpenGLEmulator::GetTextureArena(FirstBlock, NumBlocks);
			SceneRenderer.GetTextureCache().SetArena(FirstBlock, NumBlocks);
			// Uncooked textures draw as the PS2 cook makes them (cooked ones already are).
			SceneRenderer.GetTextureCache().SetTextureConverter(&ConvertTextureAsPS2Cook);
			Settings = FRendererSettings::Load();
			SceneRenderer.GetTextureCache().SetUploadBudgetKB(Settings.TextureUploadBudgetKB);
			SceneRenderer.SetLODDistanceScale(Settings.StaticMeshLODDistanceScale);
			bInitialized = true;
			UE_LOG(LogRenderer, Log,
				"Renderer: the GS scene renderer on the OpenGL GS emulator (%dx%d shown at %.3f, a frame every %d "
				"field(s))",
				FGSOpenGLEmulator::FrameWidth, FGSOpenGLEmulator::FrameHeight, double(Settings.DisplayAspectRatio),
				Settings.SyncInterval);
			return true;
		}

		void ShutdownRenderer() override
		{
			SceneRenderer.GetTextureCache().Reset();
			Emulator.Shutdown();
			bInitialized = false;
		}

		void ReleaseAssetResources(const UObject* Asset) override
		{
			// The emulator executes each list as it is recorded: no list holds the asset's data after this.
			(void)SceneRenderer.ReleaseAssetResources(Asset);
		}

		[[nodiscard]] bool IsRendererInitialized() const override
		{
			return bInitialized;
		}

		[[nodiscard]] FSceneInterface* AllocateScene(UWorld* World) override
		{
			FScene* Scene = new FScene(World);
			Scenes.Add(Scene);
			return Scene;
		}

		void RemoveScene(FSceneInterface* Scene) override
		{
			if (Scene != nullptr && Scenes.Remove(Scene) > 0)
			{
				delete Scene;
			}
		}

		void BeginRenderingViewFamily(FCanvas* Canvas, FSceneViewFamily* ViewFamily) override
		{
			(void)Canvas;
			if (!bInitialized || ViewFamily == nullptr)
			{
				return;
			}
			List.Reset();
			if (!bEnvironmentSet)
			{
				// The emulator starts without registers: the environment once, as the PS2 RHI sets it at start.
				FGSOpenGLEmulator::GetDrawEnvironment().Append(List);
				bEnvironmentSet = true;
			}
			SceneRenderer.Render(*ViewFamily, FGSOpenGLEmulator::GetDrawEnvironment(), List);
			Emulator.Execute(List);
		}

		void DrawCanvas(const FCanvas& Canvas) override
		{
			if (!bInitialized)
			{
				return;
			}
			List.Reset();
			SceneRenderer.DrawCanvas(Canvas, FGSOpenGLEmulator::GetDrawEnvironment(), List);
			Emulator.Execute(List);
		}

		[[nodiscard]] FIntPoint GetRenderTargetSize(const FIntPoint& WindowSize) const override
		{
			(void)WindowSize;
			return FIntPoint(FGSOpenGLEmulator::FrameWidth, FGSOpenGLEmulator::FrameHeight);
		}

		[[nodiscard]] float GetDisplayAspectRatio(const FIntPoint& TargetSize) const override
		{
			return Settings.GetDisplayAspectRatio(TargetSize);
		}

		[[nodiscard]] FVector2D WindowToRenderTarget(
			const FVector2D& WindowPosition, const FIntPoint& WindowSize) const override
		{
			// The inverse of Present: the frame scaled into the window's centre at the display's aspect.
			int32 X = 0;
			int32 Y = 0;
			int32 Width = 0;
			int32 Height = 0;
			FGSOpenGLEmulator::GetPresentRect(WindowSize.X, WindowSize.Y,
				Settings.GetDisplayAspectRatio(
					FIntPoint(FGSOpenGLEmulator::FrameWidth, FGSOpenGLEmulator::FrameHeight)),
				X, Y, Width, Height);
			if (Width <= 0 || Height <= 0)
			{
				return WindowPosition;
			}
			return FVector2D((WindowPosition.X - float(X)) * float(FGSOpenGLEmulator::FrameWidth) / float(Width),
				(WindowPosition.Y - float(Y)) * float(FGSOpenGLEmulator::FrameHeight) / float(Height));
		}

		void EndDrawingViewport(const FIntPoint& WindowSize) override
		{
			if (bInitialized)
			{
				FramePacer.Wait(Settings.SyncInterval);
				Emulator.Present(WindowSize.X, WindowSize.Y,
					Settings.GetDisplayAspectRatio(
						FIntPoint(FGSOpenGLEmulator::FrameWidth, FGSOpenGLEmulator::FrameHeight)));
			}
		}

		EShaderReloadResult ReloadShaders(bool bForce) override
		{
			return bInitialized ? Emulator.ReloadShaders(bForce) : EShaderReloadResult::Unchanged;
		}

		[[nodiscard]] const FFrameStats& GetFrameStats() const override
		{
			return SceneRenderer.GetFrameStats();
		}

		void ReadFramebufferBgr(int32 Width, int32 Height, TArray<uint8>& OutBgr) const override
		{
			OutBgr.Reset();
			if (!bInitialized)
			{
				return;
			}
			// The emulated frame, bottom row first.
			const TArray<FColor> Pixels = Emulator.ReadFrame(Width, Height);
			if (Pixels.Num() != Width * Height)
			{
				return;
			}
			OutBgr.SetNumUninitialized(Width * Height * 3);
			for (int32 Row = 0; Row < Height; ++Row)
			{
				const FColor* Source = &Pixels[(Height - 1 - Row) * Width];
				uint8* Target = &OutBgr[Row * Width * 3];
				for (int32 X = 0; X < Width; ++X)
				{
					Target[(X * 3) + 0] = Source[X].B;
					Target[(X * 3) + 1] = Source[X].G;
					Target[(X * 3) + 2] = Source[X].R;
				}
			}
		}

		void ShutdownModule() override
		{
			for (FSceneInterface* Scene : Scenes)
			{
				delete Scene;
			}
			Scenes.Empty();
		}

	private:
		TArray<FSceneInterface*> Scenes;
		FGSSceneRenderer SceneRenderer;
		/** Mutable: reading the frame back flushes the emulator's pending draws. */
		mutable FGSOpenGLEmulator Emulator;
		/** The list each call records into (kept, so its capacity is reused frame after frame). */
		FGSCommandList List;
		FRendererSettings Settings;
		FFramePacer FramePacer;
		bool bInitialized = false;
		bool bEnvironmentSet = false;
	};

} // namespace

IMPLEMENT_MODULE(FRendererModule, Renderer)
