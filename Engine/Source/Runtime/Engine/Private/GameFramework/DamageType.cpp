#include "GameFramework/DamageType.h"

UDamageType::UDamageType(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCausedByWorld = false;
	bScaleMomentumByMass = true;
	bRadialDamageVelChange = false;
}
