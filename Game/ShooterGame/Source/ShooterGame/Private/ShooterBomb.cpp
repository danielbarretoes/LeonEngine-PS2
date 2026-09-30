#include "ShooterBomb.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "ShooterCharacter.h"
#include "ShooterGame.h"
#include "ShooterGameMode.h"
#include "ShooterPlayerController.h"
#include "Sound/SoundWave.h"
#include "TimerManager.h"

namespace
{

	/** The explosion's flash. */
	constexpr float ExplosionLightIntensity = 20.0f;
	constexpr float ExplosionLightLifeSpan = 0.3f;

	/** The explosion's centre above the bomb, cm. */
	constexpr float ExplosionLift = 20.0f;

	/** Beeps: one a second, down to BeepFastest over the last BeepSpeedUpTime seconds. */
	constexpr float BeepSlowest = 1.0f;
	constexpr float BeepFastest = 0.15f;
	constexpr float BeepSpeedUpTime = 10.0f;

	AShooterGameMode* GetShooterGameMode(const UWorld* World)
	{
		return World != nullptr ? World->GetAuthGameMode<AShooterGameMode>() : nullptr;
	}

} // namespace

AShooterBomb::AShooterBomb(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BombMesh"));
	Mesh->SetupAttachment(GetRootComponent());
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetVisibility(false);
	SetCanBeDamaged(false);
}

void AShooterBomb::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (UStaticMesh* BombMesh = LoadShooterAsset<UStaticMesh>(MeshName))
	{
		(void)Mesh->SetStaticMesh(BombMesh);
	}
	BeepSound = LoadShooterAsset<USoundWave>(BeepSoundName);
	PlantSound = LoadShooterAsset<USoundWave>(PlantSoundName);
	DefuseSound = LoadShooterAsset<USoundWave>(DefuseSoundName);
	ExplodeSound = LoadShooterAsset<USoundWave>(ExplodeSoundName);
}

void AShooterBomb::BeginPlay()
{
	Super::BeginPlay();
	if (AShooterGameMode* GameMode = GetShooterGameMode(GetWorld()))
	{
		GameMode->RegisterBomb(this);
	}
}

float AShooterBomb::GetWorldTime() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetTimeSeconds() : 0.0f;
}

float AShooterBomb::GetTimerEndTime(FTimerHandle Handle) const
{
	const UWorld* World = GetWorld();
	const float Remaining = World != nullptr ? World->GetTimerManager().GetTimerRemaining(Handle) : -1.0f;
	return Remaining >= 0.0f ? GetWorldTime() + Remaining : 0.0f;
}

float AShooterBomb::GetDefuseEndTime() const
{
	return GetTimerEndTime(TimerHandle_Defuse);
}

void AShooterBomb::PlaySound(USoundWave* Sound) const
{
	if (Sound != nullptr)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
	}
}

void AShooterBomb::GiveTo(AShooterCharacter* NewCarrier)
{
	if (NewCarrier == nullptr || State == EShooterBombState::Planted || State == EShooterBombState::Defused ||
		State == EShooterBombState::Exploded)
	{
		return;
	}
	if (AShooterGameMode* GameMode = GetShooterGameMode(GetWorld()))
	{
		GameMode->UnregisterPickup(this);
	}
	Carrier = NewCarrier;
	Carrier->SetCarriedBomb(this);
	State = EShooterBombState::Carried;
	Mesh->SetVisibility(false);
	(void)SetActorLocation(Carrier->GetActorLocation());
	if (AShooterGameMode* GameMode = GetShooterGameMode(GetWorld()))
	{
		GameMode->OnBombStateChanged(this);
	}
}

void AShooterBomb::Drop(const FVector& Location)
{
	if (State != EShooterBombState::Carried)
	{
		return;
	}
	Dropper = Carrier;
	DropperPickupTime = GetWorldTime() + PickupDelay;
	State = EShooterBombState::Dropped;
	Carrier = nullptr;
	if (Dropper != nullptr)
	{
		Dropper->SetCarriedBomb(nullptr);
	}
	(void)SetActorLocationAndRotation(Location, FRotator::ZeroRotator);
	Mesh->SetVisibility(true);
	UE_LOG(LogShooter, Log, TEXT("The bomb was dropped at (%.0f, %.0f, %.0f)"), static_cast<double>(Location.X),
		static_cast<double>(Location.Y), static_cast<double>(Location.Z));
	if (AShooterGameMode* GameMode = GetShooterGameMode(GetWorld()))
	{
		GameMode->RegisterPickup(this);
		GameMode->OnBombStateChanged(this);
	}
}

void AShooterBomb::Plant(const FVector& Location, FName InSite, AShooterCharacter* Planter)
{
	if (State != EShooterBombState::Carried)
	{
		return;
	}
	if (Carrier != nullptr)
	{
		Carrier->SetCarriedBomb(nullptr);
	}
	Carrier = nullptr;
	State = EShooterBombState::Planted;
	Site = InSite;
	ExplodeTime = GetWorldTime() + BombTimer;
	FTimerManager& TimerManager = GetWorldTimerManager();
	TimerManager.SetTimer(TimerHandle_Explode, this, &AShooterBomb::OnExplodeTimer, BombTimer);
	// The first beep at the next step.
	TimerManager.SetTimer(TimerHandle_Beep, this, &AShooterBomb::Beep, BeepSlowest, false, 0.0f);
	(void)SetActorLocationAndRotation(Location, FRotator::ZeroRotator);
	Mesh->SetVisibility(true);
	PlaySound(PlantSound);
	if (AShooterGameMode* GameMode = GetShooterGameMode(GetWorld()))
	{
		GameMode->OnBombPlanted(this, Planter);
	}
}

bool AShooterBomb::StartDefuse(AShooterCharacter* NewDefuser)
{
	if (State != EShooterBombState::Planted || NewDefuser == nullptr || Defuser != nullptr ||
		!CanKeepDefusing(*NewDefuser))
	{
		return false;
	}
	Defuser = NewDefuser;
	GetWorldTimerManager().SetTimer(TimerHandle_Defuse, this, &AShooterBomb::OnDefuseTimer,
		NewDefuser->HasDefuseKit() ? DefuseKitDuration : DefuseDuration);
	UE_LOG(LogShooter, Log, TEXT("%s is defusing the bomb%s"), *NewDefuser->GetName(),
		NewDefuser->HasDefuseKit() ? TEXT(" with a kit") : TEXT(""));
	return true;
}

void AShooterBomb::StopDefuse(AShooterCharacter* OldDefuser)
{
	if (Defuser != nullptr && Defuser == OldDefuser)
	{
		Defuser = nullptr;
		GetWorldTimerManager().ClearTimer(TimerHandle_Defuse);
	}
}

bool AShooterBomb::CanKeepDefusing(const AShooterCharacter& Pawn) const
{
	if (!Pawn.IsAlive() || Pawn.IsPendingKillPending() || Pawn.GetTeam() != EShooterTeam::CT)
	{
		return false;
	}
	const FVector Delta = Pawn.GetActorLocation() - GetActorLocation();
	constexpr float DefuseReachZ = 150.0f;
	return Delta.SizeSquared2D() <= FMath::Square(DefuseRadius) && FMath::Abs(Delta.Z) <= DefuseReachZ;
}

void AShooterBomb::Explode()
{
	if (State != EShooterBombState::Planted)
	{
		return;
	}
	State = EShooterBombState::Exploded;
	FTimerManager& TimerManager = GetWorldTimerManager();
	TimerManager.ClearTimer(TimerHandle_Explode);
	TimerManager.ClearTimer(TimerHandle_Beep);
	TimerManager.ClearTimer(TimerHandle_Defuse);
	// A defuser who lives through the blast is not defusing any more.
	if (AShooterCharacter* OldDefuser = Defuser)
	{
		OldDefuser->StopUse();
	}
	Defuser = nullptr;
	const FVector Origin = GetActorLocation() + FVector(0.0f, 0.0f, ExplosionLift);
	// CS's bomb reaches through walls: no line-of-sight channel.
	const TArray<AActor*> IgnoreActors;
	(void)UGameplayStatics::ApplyRadialDamageWithFalloff(this, ExplosionDamage, 0.0f, Origin, 0.0f, ExplosionRadius,
		1.0f, UDamageType::StaticClass(), IgnoreActors, this, nullptr, ECC_MAX);
	(void)UGameplayStatics::SpawnPointLightAtLocation(this, Origin, FLinearColor(1.0f, 0.55f, 0.2f),
		ExplosionLightIntensity, ExplosionRadius, ExplosionLightLifeSpan);
	PlaySound(ExplodeSound);
	AShooterPlayerController::PlayExplosionForceFeedback(GetWorld(), Origin, ExplosionRadius);
	Mesh->SetVisibility(false);
	UE_LOG(LogShooter, Log, TEXT("The bomb exploded at site %s"), *Site.ToString());
	if (AShooterGameMode* GameMode = GetShooterGameMode(GetWorld()))
	{
		GameMode->OnBombExploded(this);
	}
}

void AShooterBomb::Beep()
{
	if (State != EShooterBombState::Planted)
	{
		return;
	}
	PlaySound(BeepSound);
	FTimerManager& TimerManager = GetWorldTimerManager();
	const float Left = TimerManager.GetTimerRemaining(TimerHandle_Explode);
	const float Alpha = FMath::Clamp(1.0f - (Left / BeepSpeedUpTime), 0.0f, 1.0f);
	TimerManager.SetTimer(TimerHandle_Beep, this, &AShooterBomb::Beep, FMath::Lerp(BeepSlowest, BeepFastest, Alpha));
}

void AShooterBomb::OnExplodeTimer()
{
	// CS: a defuse that ends no later than the explosion wins (its timer is due on this step too).
	if (Defuser != nullptr && GetWorldTimerManager().GetTimerRemaining(TimerHandle_Defuse) == 0.0f)
	{
		OnDefuseTimer();
		return;
	}
	Explode();
}

void AShooterBomb::OnDefuseTimer()
{
	if (State != EShooterBombState::Planted || Defuser == nullptr)
	{
		return;
	}
	AShooterCharacter* Hero = Defuser;
	State = EShooterBombState::Defused;
	Defuser = nullptr;
	FTimerManager& TimerManager = GetWorldTimerManager();
	TimerManager.ClearTimer(TimerHandle_Defuse);
	TimerManager.ClearTimer(TimerHandle_Explode);
	TimerManager.ClearTimer(TimerHandle_Beep);
	PlaySound(DefuseSound);
	UE_LOG(LogShooter, Log, TEXT("%s defused the bomb"), *Hero->GetName());
	Hero->StopUse();
	if (AShooterGameMode* GameMode = GetShooterGameMode(GetWorld()))
	{
		GameMode->OnBombDefused(this, Hero);
	}
}

void AShooterBomb::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Carrier != nullptr && Carrier->GetCarriedBomb() == this)
	{
		Carrier->SetCarriedBomb(nullptr);
	}
	Carrier = nullptr;
	if (AShooterGameMode* GameMode = GetShooterGameMode(GetWorld()))
	{
		GameMode->UnregisterPickup(this);
		GameMode->UnregisterBomb(this);
	}
	Super::EndPlay(EndPlayReason);
}

void AShooterBomb::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* World = GetWorld();
	switch (State)
	{
		case EShooterBombState::Carried:
			if (Carrier != nullptr)
			{
				(void)SetActorLocation(Carrier->GetActorLocation());
			}
			break;
		case EShooterBombState::Dropped:
			if (const AShooterGameMode* GameMode = GetShooterGameMode(World))
			{
				// The game mode's pawns, in the level's order (GiveTo changes no pawn).
				for (AShooterCharacter* Pawn : GameMode->GetPawns())
				{
					if (Pawn->IsPendingKillPending() || !Pawn->IsAlive() || Pawn->GetTeam() != EShooterTeam::T ||
						(Pawn == Dropper && GetWorldTime() < DropperPickupTime))
					{
						continue;
					}
					const FVector Delta = Pawn->GetActorLocation() - GetActorLocation();
					if (Delta.SizeSquared2D() <= FMath::Square(PickupRadius) && FMath::Abs(Delta.Z) <= 100.0f)
					{
						UE_LOG(LogShooter, Log, TEXT("%s picked up the bomb"), *Pawn->GetName());
						GiveTo(Pawn);
						Pawn->NotifyPickup(TEXT("C4"));
						break;
					}
				}
			}
			break;
		case EShooterBombState::Planted:
			// A defuser who leaves, lets go or dies stops (the defuse's timer goes with it).
			if (Defuser != nullptr && !CanKeepDefusing(*Defuser))
			{
				Defuser->StopUse();
				StopDefuse(Defuser);
			}
			break;
		case EShooterBombState::None:
		case EShooterBombState::Defused:
		case EShooterBombState::Exploded:
			break;
	}
}
