#include "ShooterBomb.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "ShooterCharacter.h"
#include "ShooterGame.h"
#include "ShooterGameMode.h"
#include "Sound/SoundWave.h"

namespace
{

	template <class T>
	T* LoadOptionalAsset(const FSoftObjectPath& Path)
	{
		if (Path.IsNull() || !FPackageName::DoesPackageExist(Path.GetLongPackageName()))
		{
			return nullptr;
		}
		return Cast<T>(Path.TryLoad());
	}

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
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BombMesh"));
	Mesh->SetupAttachment(GetRootComponent());
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetVisibility(false);
	SetCanBeDamaged(false);
}

void AShooterBomb::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (UStaticMesh* BombMesh = LoadOptionalAsset<UStaticMesh>(MeshName))
	{
		(void)Mesh->SetStaticMesh(BombMesh);
	}
	BeepSound = LoadOptionalAsset<USoundWave>(BeepSoundName);
	PlantSound = LoadOptionalAsset<USoundWave>(PlantSoundName);
	DefuseSound = LoadOptionalAsset<USoundWave>(DefuseSoundName);
	ExplodeSound = LoadOptionalAsset<USoundWave>(ExplodeSoundName);
}

float AShooterBomb::GetWorldTime() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetTimeSeconds() : 0.0f;
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
	if (Carrier != nullptr)
	{
		Carrier->SetCarriedBomb(nullptr);
	}
	Carrier = nullptr;
	State = EShooterBombState::Dropped;
	(void)SetActorLocationAndRotation(Location, FRotator::ZeroRotator);
	Mesh->SetVisibility(true);
	UE_LOG(LogShooter, Log, TEXT("The bomb was dropped at (%.0f, %.0f, %.0f)"), static_cast<double>(Location.X),
		static_cast<double>(Location.Y), static_cast<double>(Location.Z));
	if (AShooterGameMode* GameMode = GetShooterGameMode(GetWorld()))
	{
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
	const float Now = GetWorldTime();
	ExplodeTime = Now + BombTimer;
	NextBeepTime = Now;
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
	DefuseEndTime = GetWorldTime() + (NewDefuser->HasDefuseKit() ? DefuseKitDuration : DefuseDuration);
	UE_LOG(LogShooter, Log, TEXT("%s is defusing the bomb%s"), *NewDefuser->GetName(),
		NewDefuser->HasDefuseKit() ? TEXT(" with a kit") : TEXT(""));
	return true;
}

void AShooterBomb::StopDefuse(AShooterCharacter* OldDefuser)
{
	if (Defuser != nullptr && Defuser == OldDefuser)
	{
		Defuser = nullptr;
		DefuseEndTime = 0.0f;
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
	Defuser = nullptr;
	const FVector Origin = GetActorLocation() + FVector(0.0f, 0.0f, ExplosionLift);
	// CS's bomb reaches through walls: no line-of-sight channel.
	const TArray<AActor*> IgnoreActors;
	(void)UGameplayStatics::ApplyRadialDamageWithFalloff(this, ExplosionDamage, 0.0f, Origin, 0.0f, ExplosionRadius,
		1.0f, UDamageType::StaticClass(), IgnoreActors, this, nullptr, ECC_MAX);
	(void)UGameplayStatics::SpawnPointLightAtLocation(this, Origin, FLinearColor(1.0f, 0.55f, 0.2f),
		ExplosionLightIntensity, ExplosionRadius, ExplosionLightLifeSpan);
	PlaySound(ExplodeSound);
	Mesh->SetVisibility(false);
	UE_LOG(LogShooter, Log, TEXT("The bomb exploded at site %s"), *Site.ToString());
	if (AShooterGameMode* GameMode = GetShooterGameMode(GetWorld()))
	{
		GameMode->OnBombExploded(this);
	}
}

void AShooterBomb::TickBeeps(float Now)
{
	if (Now < NextBeepTime)
	{
		return;
	}
	PlaySound(BeepSound);
	const float Left = ExplodeTime - Now;
	const float Alpha = FMath::Clamp(1.0f - (Left / BeepSpeedUpTime), 0.0f, 1.0f);
	NextBeepTime = Now + FMath::Lerp(BeepSlowest, BeepFastest, Alpha);
}

void AShooterBomb::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = GetWorldTime();
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
			if (World != nullptr && World->PersistentLevel != nullptr)
			{
				for (AActor* Actor : World->PersistentLevel->Actors)
				{
					AShooterCharacter* Pawn = Cast<AShooterCharacter>(Actor);
					if (Pawn == nullptr || Pawn->IsPendingKillPending() || !Pawn->IsAlive() ||
						Pawn->GetTeam() != EShooterTeam::T)
					{
						continue;
					}
					const FVector Delta = Pawn->GetActorLocation() - GetActorLocation();
					if (Delta.SizeSquared2D() <= FMath::Square(PickupRadius) && FMath::Abs(Delta.Z) <= 100.0f)
					{
						UE_LOG(LogShooter, Log, TEXT("%s picked up the bomb"), *Pawn->GetName());
						GiveTo(Pawn);
						break;
					}
				}
			}
			break;
		case EShooterBombState::Planted:
			if (Defuser != nullptr && !CanKeepDefusing(*Defuser))
			{
				Defuser->StopUse();
				StopDefuse(Defuser);
			}
			if (Defuser != nullptr && Now >= DefuseEndTime && DefuseEndTime <= ExplodeTime)
			{
				AShooterCharacter* Hero = Defuser;
				State = EShooterBombState::Defused;
				Defuser = nullptr;
				PlaySound(DefuseSound);
				UE_LOG(LogShooter, Log, TEXT("%s defused the bomb"), *Hero->GetName());
				Hero->StopUse();
				if (AShooterGameMode* GameMode = GetShooterGameMode(World))
				{
					GameMode->OnBombDefused(this, Hero);
				}
				break;
			}
			if (Now >= ExplodeTime)
			{
				Explode();
				break;
			}
			TickBeeps(Now);
			break;
		case EShooterBombState::None:
		case EShooterBombState::Defused:
		case EShooterBombState::Exploded:
			break;
	}
}
