#pragma once

#include "CoreMinimal.h"
#include "Perception/PawnSensingComponent.h"
#include "ShooterPawnSensingComponent.generated.h"

/**
 * A bot's senses (UPawnSensingComponent) that only look at and listen to what the bot acts on (ps2-shipping N20; UE's
 * PawnSensing has no team filter, so a game narrows ShouldCheckVisibilityOf): the sight checks (the range, the cone
 * and the line of sight traces) run for the living shooters of the other team, and only while the bot's own pawn is
 * alive and not frozen; the hearing takes the other team's noises. These are exactly the pawns and noises
 * AShooterAIController's OnSeePawn and OnHearNoise did not ignore, so the bots decide the same with fewer traces: none
 * to teammates, corpses or the spectator.
 *
 * A look of a living, unfrozen bot also takes one of the frame's looks from the game mode
 * (AShooterGameMode::ClaimSensingUpdate): when the frame has none left for it, it waits in turn for the next frame.
 *
 * The grenades (ps2-shipping N30b): a bot blinded by a flashbang (AShooterCharacter::IsBlind) looks at nobody, and a
 * smoke grenade's thick cloud across the line of sight hides what is behind it
 * (AShooterGameMode::IsSightBlockedBySmoke).
 */
UCLASS()
class SHOOTERGAME_API UShooterPawnSensingComponent : public UPawnSensingComponent
{
	GENERATED_BODY()

public:
	UShooterPawnSensingComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** A living shooter of another team than the bot's pawn, which is alive, not frozen and not blind. */
	[[nodiscard]] bool ShouldCheckVisibilityOf(const APawn* Pawn) const override;
	/** UPawnSensingComponent's line of sight, unless a smoke cloud lies across it. */
	[[nodiscard]] bool HasLineOfSightTo(const AActor* Other) const override;
	/** A shooter of another team than the bot's pawn. */
	[[nodiscard]] bool ShouldCheckAudibilityOf(const APawn* NoiseInstigator) const override;

protected:
	/** Looks when the game mode gives it one of the frame's looks, else asks again the next frame. */
	void OnTimer() override;
	/** Leaves the game mode's queue of looks. */
	void OnUnregister() override;
};
