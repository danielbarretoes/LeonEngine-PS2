#include <algorithm>
#include "Engine/Level.h"
#include <string_view>
#include <utility>


std::size_t UStaticMeshComponent::subMeshCount() const {
    if (mesh == nullptr || !mesh->Valid()) {
        return 0;
    }
    return mesh->GetSubmeshes().empty() ? 1 : mesh->GetSubmeshes().size();
}

const FMaterial& UStaticMeshComponent::materialForSubMesh(std::size_t subMeshIndex) const {
    int slot = 0;
    if (mesh != nullptr && subMeshIndex < mesh->GetSubmeshes().size()) {
        slot = mesh->GetSubmeshes()[subMeshIndex].MaterialIndex;
    }

    if (slot >= 0 && static_cast<std::size_t>(slot) < materials.size()) {
        return materials[static_cast<std::size_t>(slot)];
    }
    if (materialOverride) {
        return material;
    }
    if (mesh != nullptr && mesh->HasMaterials() && slot >= 0 &&
        static_cast<std::size_t>(slot) < mesh->GetMaterials().size()) {
        return mesh->GetMaterials()[static_cast<std::size_t>(slot)];
    }
    return material;
}

bool UStaticMeshComponent::isShadowCaster() const {
    if (hidden || mesh == nullptr || !mesh->Valid()) {
        return false;
    }

    const auto countsAsCaster = [](const FMaterial& mat) {
        return mat.bCastsShadows && !mat.IsTransparent() && mat.Shading != EMaterialShadingModel::Unlit;
    };

    if (!materials.empty()) {
        return std::any_of(materials.begin(), materials.end(), countsAsCaster);
    }
    if (materialOverride) {
        return countsAsCaster(material);
    }
    if (mesh->HasMaterials()) {
        const auto& mats = mesh->GetMaterials();
        return std::any_of(mats.begin(), mats.end(), countsAsCaster);
    }
    return countsAsCaster(material);
}

UStaticMeshComponent& ULevel::AddStaticMesh(UStaticMeshComponent component) {
    staticMeshes_.push_back(std::move(component));
    return staticMeshes_.back();
}

FPlayerStart& ULevel::AddPlayerStart(FPlayerStart start) {
    playerStarts_.push_back(std::move(start));
    return playerStarts_.back();
}

FTriggerVolume& ULevel::AddTriggerVolume(FTriggerVolume volume) {
    triggerVolumes_.push_back(std::move(volume));
    return triggerVolumes_.back();
}

FPainCausingVolume& ULevel::AddPainCausingVolume(FPainCausingVolume volume) {
    painCausingVolumes_.push_back(std::move(volume));
    return painCausingVolumes_.back();
}

FAISpawnPoint& ULevel::AddAISpawnPoint(FAISpawnPoint point) {
    aiSpawnPoints_.push_back(std::move(point));
    return aiSpawnPoints_.back();
}

const FPlayerStart* ULevel::FindPlayerStart() const {
    if (playerStarts_.empty()) {
        return nullptr;
    }
    return &playerStarts_.front();
}

std::size_t ULevel::FindStaticMeshIndexByTag(std::string_view tag) const {
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

void ULevel::ClearStaticMeshes() {
    staticMeshes_.clear();
}

void ULevel::ClearPlayerStarts() {
    playerStarts_.clear();
}

void ULevel::ClearTriggerVolumes() {
    triggerVolumes_.clear();
}

void ULevel::ClearPainCausingVolumes() {
    painCausingVolumes_.clear();
}

void ULevel::ClearAISpawnPoints() {
    aiSpawnPoints_.clear();
}

void ULevel::ClearLights() {
    directionalLights_.clear();
    pointLights_.clear();
}

void ULevel::Clear() {
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

