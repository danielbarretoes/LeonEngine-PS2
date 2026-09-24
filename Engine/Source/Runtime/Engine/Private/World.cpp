#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "BodyInstance.h"
#include "Renderer.h"
#include <vector>

namespace leon {

void World::Tick(float deltaTime) {
    ticking_ = true;
    for (auto& actor : actors_) {
        if (actor && !actor->IsPendingKillPending()) {
            actor->TickComponents(deltaTime);
            actor->Tick(deltaTime);
        }
    }
    ticking_ = false;
    flushPendingSpawns();
    purgePending();
}

void World::RegisterBodiesFromLevel(const Level& level) {
    physics_.Clear();
    const auto& meshes = level.StaticMeshes();
    for (std::size_t i = 0; i < meshes.size(); ++i) {
        const StaticMeshComponent& component = meshes[i];
        if (!component.HasPhysicsBody()) {
            continue;
        }
        BodyInstanceDesc desc{};
        desc.levelMeshIndex = i;
        desc.type = component.simulatePhysics ? EBodyType::Dynamic : EBodyType::Static;
        desc.enableGravity = component.enableGravity;
        physics_.AddBody(desc);
    }
}

void World::resolveCharacterOverlaps() {
    std::vector<Character*> characters;
    characters.reserve(actors_.size());
    ForEach<Character>([&](Character& character) { characters.push_back(&character); });
    if (characters.size() < 2) {
        return;
    }

    // Flow: collect live Characters → iterate pairs → equal XZ depenetration (2–3 passes).
    constexpr int kIterations = 3;
    for (int iter = 0; iter < kIterations; ++iter) {
        for (std::size_t i = 0; i < characters.size(); ++i) {
            for (std::size_t j = i + 1; j < characters.size(); ++j) {
                characters[i]->ResolvePawnOverlap(*characters[j]);
            }
        }
    }
}

void World::TickGameplayFrame(const WorldGameplayFrameParams& params) {
    ForEach<Character>([&](Character& character) {
        character.TickCharacterMovement(params.deltaTime, params.collisionDebugDraw);
    });
    resolveCharacterOverlaps();

    PhysSceneStepParams step{};
    step.deltaTime = params.deltaTime;
    if (params.overridePhysicsStep) {
        step.damping = params.physicsDamping;
        step.walkBounds = params.physicsWalkBounds;
        step.gravity = params.physicsGravity;
        step.floorY = params.physicsFloorY;
        step.skin = params.physicsSkin;
    } else if (Character* primary = FindFirst<Character>()) {
        const CharacterMovement& moveCfg = primary->GetCharacterMovement();
        step.damping = moveCfg.PushDamping;
        step.walkBounds = moveCfg.WalkBounds;
        step.gravity = moveCfg.Gravity;
        step.floorY = moveCfg.FloorY;
        step.skin = moveCfg.Skin;
        step.skipLevelMeshIndex = primary->LevelMeshIndex();
    }
    physics_.Step(step);

    ForEach<Character>([](Character& character) { character.ResolveOverlaps(); });
    resolveCharacterOverlaps();

    Tick(params.deltaTime);

    if (params.level != nullptr) {
        physics_.SyncToLevel(*params.level);
        ForEach<Character>([level = params.level](Character& character) {
            character.SyncTransformToLevel(*level);
        });
    }

    if (params.renderer != nullptr) {
        SubmitSkeletalDraws(*params.renderer);
    }

    if (params.collisionDebugDraw != nullptr) {
        ForEach<Character>([&](Character& character) {
            physics_.AppendCollisionDebug(*params.collisionDebugDraw, character.GetCapsule(),
                                          character.GetActorLocation(), character.LevelMeshIndex());
        });
    }

    if (params.navMeshDebugDraw != nullptr) {
        navigation_.AppendDebugDraw(*params.navMeshDebugDraw);
    }
}

void World::SubmitSkeletalDraws(Renderer& renderer) const {
    ForEach<Character>([&](Character& character) { character.SubmitMeshDraw(renderer); });
}

void World::Clear() {
    for (auto& actor : pendingSpawns_) {
        if (actor) {
            actor->world_ = nullptr;
        }
    }
    pendingSpawns_.clear();
    for (auto& actor : actors_) {
        if (actor) {
            actor->EndPlay();
            actor->EndPlayComponents();
            actor->world_ = nullptr;
        }
    }
    actors_.clear();
    physics_.Clear();
}

void World::flushPendingSpawns() {
    for (auto& owned : pendingSpawns_) {
        if (!owned) {
            continue;
        }
        Actor* raw = owned.get();
        actors_.push_back(std::move(owned));
        raw->BeginPlayComponents();
        raw->BeginPlay();
    }
    pendingSpawns_.clear();
}

void World::purgePending() {
    for (auto it = actors_.begin(); it != actors_.end();) {
        if (!*it || (*it)->IsPendingKillPending()) {
            if (*it) {
                (*it)->EndPlay();
                (*it)->EndPlayComponents();
                (*it)->world_ = nullptr;
            }
            it = actors_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace leon
