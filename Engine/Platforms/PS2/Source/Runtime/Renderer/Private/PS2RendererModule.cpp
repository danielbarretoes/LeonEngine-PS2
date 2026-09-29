#include "CanvasTypes.h"
#include "GS/GSSceneRenderer.h"
#include "Modules/ModuleManager.h"
#include "PS2RHI.h"
#include "PS2VU1.h"
#include "RendererInterface.h"
#include "RendererLog.h"
#include "RendererSettings.h"
#include "ScenePrivate.h"
#include "SceneView.h"

DEFINE_LOG_CATEGORY(LogRenderer);

namespace
{

	/**
	 * The Renderer module on the PS2 (UE: FRendererModule): the GS scene renderer records the view family and the
	 * canvas as GS lists against the frame's drawing environment, and FPS2RHI appends them to the frame it sends at
	 * WaitVSync. The textures live in the VRAM the display leaves (FPS2RHI::AllocateTextureArena).
	 */
	class FPS2RendererModule final : public IRendererModule
	{
	public:
		bool InitRenderer(const FString& ShaderDirectory) override
		{
			(void)ShaderDirectory;
			if (bInitialized)
			{
				return true;
			}
			uint32 FirstBlock = 0;
			uint32 NumBlocks = 0;
			if (!FPS2RHI::AllocateTextureArena(FirstBlock, NumBlocks))
			{
				UE_LOG(LogRenderer, Error, "PS2 renderer: no VRAM left for textures");
				return false;
			}
			SceneRenderer.GetTextureCache().SetArena(FirstBlock, NumBlocks);
			// The static meshes' batches inside the guard band go to VU1; -novu1 sends them through the C++ emitter.
			SceneRenderer.SetVertexBatches(FPS2VU1::IsEnabled());
			// The frame rate (Docs/PLANS/ps2-engine.md D6: a steady 30 fps) and the TV's aspect ratio, as on the
			// desktop.
			Settings = FRendererSettings::Load();
			SceneRenderer.GetTextureCache().SetUploadBudgetKB(Settings.TextureUploadBudgetKB);
			SceneRenderer.SetLODDistanceScale(Settings.StaticMeshLODDistanceScale);
			FPS2RHI::SetSyncInterval(Settings.SyncInterval);
			bInitialized = true;
			UE_LOG(LogRenderer, Log,
				"PS2 renderer: GS scene renderer, %u KB of texture VRAM, a frame every %d vertical "
				"blank(s), %s",
				NumBlocks / 4, Settings.SyncInterval,
				SceneRenderer.HasVertexBatches() ? "static meshes on VU1" : "static meshes on the EE (-novu1)");
			return true;
		}

		[[nodiscard]] float GetDisplayAspectRatio(const FIntPoint& TargetSize) const override
		{
			return Settings.GetDisplayAspectRatio(TargetSize);
		}

		void ShutdownRenderer() override
		{
			SceneRenderer.GetTextureCache().Reset();
			bInitialized = false;
		}

		void ReleaseAssetResources(const UObject* Asset) override
		{
			if (SceneRenderer.ReleaseAssetResources(Asset))
			{
				// Its levels may be in the frame's list (held in place) or in the chain being sent: copied, and waited
				// for, before the asset frees them.
				FPS2RHI::RetireInPlaceImages();
			}
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
			SceneRenderer.Render(*ViewFamily, FPS2RHI::GetDrawEnvironment(), List);
			FPS2RHI::Submit(List);
		}

		void DrawCanvas(const FCanvas& Canvas) override
		{
			if (!bInitialized)
			{
				return;
			}
			List.Reset();
			SceneRenderer.DrawCanvas(Canvas, FPS2RHI::GetDrawEnvironment(), List);
			FPS2RHI::Submit(List);
		}

		EShaderReloadResult ReloadShaders(bool bForce) override
		{
			(void)bForce;
			return EShaderReloadResult::Unchanged;
		}

		[[nodiscard]] const FFrameStats& GetFrameStats() const override
		{
			return SceneRenderer.GetFrameStats();
		}

		void ReadFramebufferBgr(int32 Width, int32 Height, TArray<uint8>& OutBgr) const override
		{
			// The GS frame is not read back (screenshots are the emulator's: PCSX2).
			(void)Width;
			(void)Height;
			OutBgr.Reset();
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
		/** The list each call records into (kept, so its capacity is reused frame after frame). */
		FGSCommandList List;
		FRendererSettings Settings;
		bool bInitialized = false;
	};

} // namespace

IMPLEMENT_MODULE(FPS2RendererModule, Renderer)
