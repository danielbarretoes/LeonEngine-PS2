#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Modules/ModuleManager.h"
#include "Perception/PawnSensingComponent.h"

namespace
{

	/**
	 * The AI module (UE: FAIModule): it sets the actors' noise delegate, so AActor::MakeNoise reaches the world's
	 * UPawnSensingComponents (UE: UAISense_Hearing's MakeNoiseDelegate), and clears it when it shuts down.
	 */
	class FAIModule : public FDefaultModuleImpl
	{
	public:
		void StartupModule() override
		{
			AActor::SetMakeNoiseDelegate(
				[](AActor* NoiseMaker, float Loudness, APawn* NoiseInstigator, const FVector& NoiseLocation)
				{
					UWorld* World = NoiseMaker != nullptr ? NoiseMaker->GetWorld() : nullptr;
					if (World != nullptr)
					{
						UPawnSensingComponent::BroadcastNoise(*World, NoiseInstigator, NoiseLocation, Loudness);
					}
				});
		}

		void ShutdownModule() override
		{
			AActor::SetMakeNoiseDelegate(FMakeNoiseDelegate());
		}
	};

} // namespace

IMPLEMENT_MODULE(FAIModule, AIModule)
