#include "PS2SceneState.h"

namespace Leon::PS2
{

	FPS2SceneState& GetSceneState()
	{
		static FPS2SceneState State{};
		return State;
	}

	void InvalidateBoundTexture()
	{
		GetSceneState().BoundTextureVram = -1;
	}

} // namespace Leon::PS2

void FPS2RHI::SetViewTarget(const FPS2ViewTarget& ViewTarget)
{
	auto& S = Leon::PS2::GetSceneState();
	S.ViewTarget = ViewTarget;
	S.bViewDirty = true;
}

void FPS2RHI::SetDirectionalLight(const FPS2DirectionalLight& Light)
{
	Leon::PS2::GetSceneState().Sun = Light;
}

void FPS2RHI::SetAmbientLightColor(float R, float G, float B)
{
	auto& S = Leon::PS2::GetSceneState();
	S.AmbientR = R;
	S.AmbientG = G;
	S.AmbientB = B;
}

void FPS2RHI::BindMaterial(const FPS2Material& Material)
{
	Leon::PS2::GetSceneState().BoundMaterial = Material;
}
