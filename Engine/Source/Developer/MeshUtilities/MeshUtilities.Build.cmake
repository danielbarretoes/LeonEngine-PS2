# MeshUtilities: glTF import (static meshes, skinned meshes and their animations, scenes) and the LPS2 v2 build of the
# meshes' render data (Unreal: Developer/MeshUtilities, which also builds for Engine's IMeshBuilderModule).
leon_module(MeshUtilities
	PLATFORMS Desktop
	PUBLIC_DEPENDENCIES Core RenderCore AnimationCore
	# Json: an animation's extras (its notifies, Docs/PLANS/ps2-shipping.md N25).
	PRIVATE_DEPENDENCIES Engine Json CGLTF MeshOptimizer
)
