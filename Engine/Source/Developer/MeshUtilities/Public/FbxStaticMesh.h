#pragma once

#include <leon/render/MeshData.h>
#include <string>

namespace leon {

/// Load a static (non-skinned) mesh from FBX via ufbx. All mesh nodes are merged with submeshes.
/// Edit-time / cook only — not part of shipping `leon_engine`.
[[nodiscard]] bool LoadStaticMeshFromFbx(const std::string& path, MeshData& out);

} // namespace leon
