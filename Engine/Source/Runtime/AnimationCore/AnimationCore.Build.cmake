# AnimationCore: the plain skeletal data under Engine's animation assets: skinned vertices, reference skeletons and baked
# clips, which the FBX importer produces (Unreal: Runtime/AnimationCore, the low-level animation types).
leon_module(AnimationCore
	PLATFORMS Desktop
	PUBLIC_DEPENDENCIES Core
)
