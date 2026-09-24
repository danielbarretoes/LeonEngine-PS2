#include <algorithm>
#include "Engine/Level.h"
#include <string_view>
#include <utility>

namespace leon {

std::size_t StaticMeshComponent::subMeshCount() const {
    if (mesh == nullptr || !mesh->Valid()) {
        return 0;
    }
    return mesh->Submeshes().empty() ? 1 : mesh->Submeshes().size();
}

const Material& StaticMeshComponent::materialForSubMesh(std::size_t subMeshIndex) const {
    int slot = 0;
    if (mesh != nullptr && subMeshIndex < mesh->Submeshes().size()) {
        slot = mesh->Submeshes()[subMeshIndex].materialIndex;
    }

    if (slot >= 0 && static_cast<std::size_t>(slot) < materials.size()) {
        return materials[static_cast<std::size_t>(slot)];
    }
    if (materialOverride) {
        return material;
    }
    if (mesh != nullptr && mesh->HasMaterials() && slot >= 0 &&
        static_cast<std::size_t>(slot) < mesh->Materials().size()) {
        return mesh->Materials()[static_cast<std::size_t>(slot)];
    }
    return material;
}

bool StaticMeshComponent::isShadowCaster() const {
    if (hidden || mesh == nullptr || !mesh->Valid()) {
        return false;
    }

    const auto countsAsCaster = [](const Material& mat) {
        return mat.castsShadows && !mat.isTransparent() && mat.shading != EShadingModel::Unlit;
    };

    if (!materials.empty()) {
        return std::any_of(materials.begin(), materials.end(), countsAsCaster);
    }
    if (materialOverride) {
        return countsAsCaster(material);
    }
    if (mesh->HasMaterials()) {
        const auto& mats = mesh->Materials();
        return std::any_of(mats.begin(), mats.end(), countsAsCaster);
    }
    return countsAsCaster(material);
}

StaticMeshComponent& Level::AddStaticMesh(StaticMeshComponent component) {
    staticMeshes_.push_back(std::move(component));
    return staticMeshes_.back();
}

PlayerStart& Level::AddPlayerStart(PlayerStart start) {
    playerStarts_.push_back(std::move(start));
    return playerStarts_.back();
}

TriggerVolume& Level::AddTriggerVolume(TriggerVolume volume) {
    triggerVolumes_.push_back(std::move(volume));
    return triggerVolumes_.back();
}

PainCausingVolume& Level::AddPainCausingVolume(PainCausingVolume volume) {
    painCausingVolumes_.push_back(std::move(volume));
    return painCausingVolumes_.back();
}

AISpawnPoint& Level::AddAISpawnPoint(AISpawnPoint point) {
    aiSpawnPoints_.push_back(std::move(point));
    return aiSpawnPoints_.back();
}

const PlayerStart* Level::FindPlayerStart() const {
    if (playerStarts_.empty()) {
        return nullptr;
    }
    return &playerStarts_.front();
}

std::size_t Level::FindStaticMeshIndexByTag(std::string_view tag) const {
    if (tag.empty()) {
        return npos;
    }
    for (std::size_t i = 0; i < staticMeshes_.size(); ++i) {
        if (staticMeshes_[i].tag == tag) {
            return i;
        }
    }
    return npos;
}

void Level::ClearStaticMeshes() {
    staticMeshes_.clear();
}

void Level::ClearPlayerStarts() {
    playerStarts_.clear();
}

void Level::ClearTriggerVolumes() {
    triggerVolumes_.clear();
}

void Level::ClearPainCausingVolumes() {
    painCausingVolumes_.clear();
}

void Level::ClearAISpawnPoints() {
    aiSpawnPoints_.clear();
}

void Level::ClearLights() {
    directionalLights_.clear();
    pointLights_.clear();
}

void Level::Clear() {
    ClearStaticMeshes();
    ClearPlayerStarts();
    ClearTriggerVolumes();
    ClearPainCausingVolumes();
    ClearAISpawnPoints();
    ClearLights();
    environment_.reset();
    environmentExposure_ = 1.0f;
    environmentPath_.clear();
    name_.clear();
    gameMode_.clear();
}

} // namespace leon
