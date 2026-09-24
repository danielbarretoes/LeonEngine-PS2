#include "GameFramework/Damage.h"

#include <glm/geometric.hpp>

#include "GameFramework/Character.h"

namespace leon {

float ApplyPointDamage(Character* DamagedActor, float BaseDamage, const glm::vec3& HitFromDirection,
                       Character* /*DamageCauser*/) {
    if (DamagedActor == nullptr || BaseDamage <= 0.0f) {
        return 0.0f;
    }
    (void)HitFromDirection;
    return DamagedActor->TakeDamage(BaseDamage);
}

float ApplyRadialDamage(const std::vector<Character*>& Actors, float BaseDamage,
                        const glm::vec3& Origin, float DamageRadius, Character* /*DamageCauser*/) {
    if (BaseDamage <= 0.0f || DamageRadius <= 0.0f) {
        return 0.0f;
    }
    float totalApplied = 0.0f;
    for (Character* actor : Actors) {
        if (actor == nullptr || !actor->IsAlive()) {
            continue;
        }
        const float dist = glm::length(actor->GetActorLocation() - Origin);
        if (dist >= DamageRadius) {
            continue;
        }
        const float falloff = 1.0f - (dist / DamageRadius);
        totalApplied += actor->TakeDamage(BaseDamage * falloff);
    }
    return totalApplied;
}

} // namespace leon
