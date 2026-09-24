#include "Kismet/GameplayStatics.h"

#include <glm/geometric.hpp>

#include "GameFramework/Character.h"


float UGameplayStatics::ApplyPointDamage(ACharacter* DamagedActor, float BaseDamage, const glm::vec3& HitFromDirection,
                       ACharacter* /*DamageCauser*/) {
    if (DamagedActor == nullptr || BaseDamage <= 0.0f) {
        return 0.0f;
    }
    (void)HitFromDirection;
    return DamagedActor->TakeDamage(BaseDamage);
}

float UGameplayStatics::ApplyRadialDamage(const std::vector<ACharacter*>& Actors, float BaseDamage,
                        const glm::vec3& Origin, float DamageRadius, ACharacter* /*DamageCauser*/) {
    if (BaseDamage <= 0.0f || DamageRadius <= 0.0f) {
        return 0.0f;
    }
    float TotalApplied = 0.0f;
    for (ACharacter* Actor : Actors) {
        if (Actor == nullptr || !Actor->IsAlive()) {
            continue;
        }
        const float Dist = glm::length(Actor->GetActorLocation() - Origin);
        if (Dist >= DamageRadius) {
            continue;
        }
        const float Falloff = 1.0f - (Dist / DamageRadius);
        TotalApplied += Actor->TakeDamage(BaseDamage * Falloff);
    }
    return TotalApplied;
}

