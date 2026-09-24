#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "BodyInstance.h"
#include "SceneRenderer.h"
#include <vector>


void UWorld::Tick(float deltaTime) {
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

void UWorld::RegisterBodiesFromLevel(const ULevel& level) {
    physics_.Clear();
    const auto& meshes = level.StaticMeshes();
    for (std::size_t i = 0; i < meshes.size(); ++i) {
        const UStaticMeshComponent& component = meshes[i];
        if (!component.HasPhysicsBody()) {
            continue;
        }
        FBodyInstanceDesc desc{};
        desc.levelMeshIndex = i;
        desc.type = component.simulatePhysics ? EBodyType::Dynamic : EBodyType::Static;
        desc.enableGravity = component.enableGravity;
        physics_.AddBody(desc);
    }
}

void UWorld::resolveCharacterOverlaps() {
    std::vector<ACharacter*> characters;
    characters.reserve(actors_.size());
    ForEach<ACharacter>([&](ACharacter& character) { characters.push_back(&character); });
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

void UWorld::TickGameplayFrame(const FWorldGameplayFrameParams& params) {
    ForEach<ACharacter>([&](ACharacter& character) {
        character.TickCharacterMovement(params.deltaTime, params.collisionDebugDraw);
    });
    resolveCharacterOverlaps();

    FPhysSceneStepParams step{};
    step.deltaTime = params.deltaTime;
    if (params.overridePhysicsStep) {
        step.damping = params.physicsDamping;
        step.walkBounds = params.physicsWalkBounds;
        step.gravity = params.physicsGravity;
        step.floorY = params.physicsFloorY;
        step.skin = params.physicsSkin;
    } else if (ACharacter* primary = FindFirst<ACharacter>()) {
        const UCharacterMovementComponent& moveCfg = primary->GetCharacterMovement();
        step.damping = moveCfg.PushDamping;
        step.walkBounds = moveCfg.WalkBounds;
        step.gravity = moveCfg.Gravity;
        step.floorY = moveCfg.FloorY;
        step.skin = moveCfg.Skin;
        step.skipLevelMeshIndex = primary->LevelMeshIndex();
    }
    physics_.Step(step);

    ForEach<ACharacter>([](ACharacter& character) { character.ResolveOverlaps(); });
    resolveCharacterOverlaps();

    Tick(params.deltaTime);

    if (params.level != nullptr) {
        physics_.SyncToLevel(*params.level);
        ForEach<ACharacter>([level = params.level](ACharacter& character) {
            character.SyncTransformToLevel(*level);
        });
    }

    if (params.renderer != nullptr) {
        SubmitSkeletalDraws(*params.renderer);
    }

    if (params.collisionDebugDraw != nullptr) {
        ForEach<ACharacter>([&](ACharacter& character) {
            physics_.AppendCollisionDebug(*params.collisionDebugDraw, character.GetCapsule(),
                                          character.GetActorLocation(), character.LevelMeshIndex());
        });
    }

    if (params.navMeshDebugDraw != nullptr) {
        navigation_.AppendDebugDraw(*params.navMeshDebugDraw);
    }
}

void UWorld::SubmitSkeletalDraws(FSceneRenderer& renderer) const {
    ForEach<ACharacter>([&](ACharacter& character) { character.SubmitMeshDraw(renderer); });
}

void UWorld::Clear() {
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

void UWorld::flushPendingSpawns() {
    for (auto& owned : pendingSpawns_) {
        if (!owned) {
            continue;
        }
        AActor* raw = owned.get();
        actors_.push_back(std::move(owned));
        raw->BeginPlayComponents();
        raw->BeginPlay();
    }
    pendingSpawns_.clear();
}

void UWorld::purgePending() {
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

