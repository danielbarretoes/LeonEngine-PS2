# MeshUtilities: Static mesh import (OBJ/FBX/glTF), skeletal FBX import and build (Unreal: Developer/MeshUtilities).
leon_module(MeshUtilities
	PLATFORMS Desktop
	PUBLIC_DEPENDENCIES Core RenderCore AnimationCore
	PRIVATE_DEPENDENCIES Renderer TinyObjLoader UFBX CGLTF
)
