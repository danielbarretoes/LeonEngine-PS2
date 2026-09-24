# MeshUtilities: Static mesh import (OBJ/FBX/glTF) and build (Unreal: Developer/MeshUtilities).
leon_module(MeshUtilities
	PLATFORMS Desktop
	PUBLIC_DEPENDENCIES Core RenderCore GLM
	PRIVATE_DEPENDENCIES Renderer TinyObjLoader UFBX CGLTF
)
