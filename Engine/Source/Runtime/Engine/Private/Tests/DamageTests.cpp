#include "Components/StaticMeshComponent.h"
#include "CoreMinimal.h"
#include "Engine/BlockingVolume.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/SpectatorPawn.h"
#include "GameFramework/SpectatorPawnMovement.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Tests/EngineTestTypes.h"
#include "Tests/ScopedTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// P18's engine pieces: UE's damage API (the events, TakeDamage, the Apply*Damage helpers and their line of sight), the
// projectile movement, the spectator pawn and state, static mesh sockets and the actors' life span.

namespace
{

	/** What an actor's damage delegates saw. */
	struct FDamageLog
	{
		int32 AnyCount = 0;
		int32 PointCount = 0;
		int32 RadialCount = 0;
		float AnyDamage = 0.0f;
		const UDamageType* DamageType = nullptr;
		AActor* Causer = nullptr;
		FVector HitLocation = FVector::ZeroVector;
		FVector ShotDirection = FVector::ZeroVector;
		FVector Origin = FVector::ZeroVector;

		void Watch(AActor& Actor)
		{
			Actor.OnTakeAnyDamage.AddLambda(
				[this](AActor*, float Damage, const UDamageType* InDamageType, AController*, AActor* InCauser)
				{
					++AnyCount;
					AnyDamage += Damage;
					DamageType = InDamageType;
					Causer = InCauser;
				});
			Actor.OnTakePointDamage.AddLambda(
				[this](AActor*, float, AController*, FVector InHitLocation, UPrimitiveComponent*, FName,
					FVector InShotDirection, const UDamageType*, AActor*)
				{
					++PointCount;
					HitLocation = InHitLocation;
					ShotDirection = InShotDirection;
				});
			Actor.OnTakeRadialDamage.AddLambda(
				[this](AActor*, float, const UDamageType*, FVector InOrigin, const FHitResult&, AController*, AActor*)
				{
					++RadialCount;
					Origin = InOrigin;
				});
		}
	};

	/** A character standing with its feet on Feet (its capsule a Pawn body of the scene). */
	ACharacter* SpawnStandingCharacter(UWorld& World, const FVector& Feet)
	{
		ACharacter* Character = World.SpawnActor<ACharacter>();
		Character->Reset(Feet);
		return Character;
	}

	/** A blocking volume: a box of 100 cm times Scale centred on Center (static, blocks every channel). */
	ABlockingVolume* SpawnWall(UWorld& World, const FVector& Center, const FVector& Scale)
	{
		return World.SpawnActor<ABlockingVolume>(
			ABlockingVolume::StaticClass(), FTransform(FQuat::Identity, Center, Scale));
	}

	/** Ticks the world Frames times at 60 Hz. */
	void TickFrames(UWorld& World, int32 Frames)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			World.Tick(1.0f / 60.0f);
		}
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDamageEventTypesTest, "System.Engine.Damage.EventTypes",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FDamageEventTypesTest::RunTest(const FString& Parameters)
{
	// UE's ClassID pattern: every event is a plain one, a point or radial event is also its own type, and a receiver
	// casts after IsOfType.
	const FDamageEvent Plain;
	FHitResult Hit;
	Hit.ImpactPoint = FVector(1.0f, 2.0f, 3.0f);
	const FPointDamageEvent Point(20.0f, Hit, FVector(1.0f, 0.0f, 0.0f), UDamageType::StaticClass());
	const FRadialDamageEvent Radial;
	TestEqual("Ids", FDamageEvent::ClassID + FPointDamageEvent::ClassID + FRadialDamageEvent::ClassID, 3);
	TestTrue("Plain is plain", Plain.IsOfType(FDamageEvent::ClassID));
	TestFalse("Plain is not a point", Plain.IsOfType(FPointDamageEvent::ClassID));
	const FDamageEvent& AsBase = Point;
	TestTrue("A point is plain too", AsBase.IsOfType(FDamageEvent::ClassID));
	TestTrue("A point is a point", AsBase.IsOfType(FPointDamageEvent::ClassID));
	TestFalse("A point is not radial", AsBase.IsOfType(FRadialDamageEvent::ClassID));
	if (AsBase.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& Cast = static_cast<const FPointDamageEvent&>(AsBase);
		TestEqual("The cast keeps the damage", Cast.Damage, 20.0f);
		FHitResult BestHit;
		FVector ImpulseDir;
		Cast.GetBestHitInfo(nullptr, nullptr, BestHit, ImpulseDir);
		TestTrue("Its hit", BestHit.ImpactPoint == Hit.ImpactPoint && ImpulseDir == FVector(1.0f, 0.0f, 0.0f));
	}
	TestTrue("Radial", static_cast<const FDamageEvent&>(Radial).GetTypeID() == FRadialDamageEvent::ClassID);
	TestTrue("The damage type", Point.DamageTypeClass == UDamageType::StaticClass());

	// UE's falloff: full within the inner radius, (1 - t)^falloff to the outer one, nothing beyond.
	const FRadialDamageParams Params(100.0f, 10.0f, 50.0f, 250.0f, 1.0f);
	TestEqual("At the origin", Params.GetDamageScale(0.0f), 1.0f);
	TestEqual("Inner radius", Params.GetDamageScale(50.0f), 1.0f);
	TestEqual("Half way", Params.GetDamageScale(150.0f), 0.5f, 1.0e-5f);
	TestEqual("Outer radius", Params.GetDamageScale(250.0f), 0.0f);
	const FRadialDamageParams Squared(100.0f, 0.0f, 50.0f, 250.0f, 2.0f);
	TestEqual("Falloff 2", Squared.GetDamageScale(150.0f), 0.25f, 1.0e-5f);
	const FRadialDamageParams Full(100.0f, 250.0f);
	TestEqual("No falloff", Full.GetDamageScale(249.0f), 1.0f);
	TestEqual("Max radius", Params.GetMaxRadius(), 250.0f);
	const UDamageType* Defaults = GetDefault<UDamageType>();
	TestTrue("UDamageType's defaults", !Defaults->bCausedByWorld && Defaults->DamageFalloff == 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDamagePointAndPlainTest, "System.Engine.Damage.PointAndPlainDamage",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FDamagePointAndPlainTest::RunTest(const FString& Parameters)
{
	// ApplyPointDamage and ApplyDamage reach AActor::TakeDamage with UE's events; the delegates see the amount, the
	// hit, the shot and the damage type; an actor that cannot be damaged takes nothing.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	ACharacter* Victim = SpawnStandingCharacter(World, FVector(300.0f, 0.0f, 0.0f));
	ACharacter* Shooter = SpawnStandingCharacter(World, FVector::ZeroVector);
	FDamageLog Log;
	Log.Watch(*Victim);

	FHitResult Hit;
	Hit.ImpactPoint = FVector(260.0f, 0.0f, 150.0f);
	Hit.Actor = Victim;
	const float Taken = UGameplayStatics::ApplyPointDamage(
		Victim, 25.0f, FVector(1.0f, 0.0f, 0.0f), Hit, nullptr, Shooter, UDamageType::StaticClass());
	TestEqual("Point damage taken", Taken, 25.0f);
	TestEqual("OnTakePointDamage", Log.PointCount, 1);
	TestEqual("OnTakeAnyDamage", Log.AnyCount, 1);
	TestTrue("The hit location", Log.HitLocation == Hit.ImpactPoint);
	TestTrue("The shot", Log.ShotDirection == FVector(1.0f, 0.0f, 0.0f));
	TestTrue("The causer", Log.Causer == Shooter);
	TestTrue("UDamageType's class default object", Log.DamageType == GetDefault<UDamageType>());

	TestEqual("Plain damage", UGameplayStatics::ApplyDamage(Victim, 10.0f, nullptr, nullptr, nullptr), 10.0f);
	TestEqual("Only OnTakeAnyDamage", Log.PointCount, 1);
	TestEqual("Any twice", Log.AnyCount, 2);
	TestEqual("Both amounts", Log.AnyDamage, 35.0f);
	TestEqual("No damage, no call", UGameplayStatics::ApplyDamage(Victim, 0.0f, nullptr, nullptr, nullptr), 0.0f);
	TestEqual("No actor", UGameplayStatics::ApplyDamage(nullptr, 5.0f, nullptr, nullptr, nullptr), 0.0f);

	Victim->SetCanBeDamaged(false);
	TestEqual("Cannot be damaged",
		UGameplayStatics::ApplyPointDamage(Victim, 25.0f, FVector(1.0f, 0.0f, 0.0f), Hit, nullptr, Shooter, nullptr),
		0.0f);
	TestEqual("No broadcast", Log.AnyCount, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDamageRadialLineOfSightTest, "System.Engine.Damage.RadialFalloffAndLineOfSight",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FDamageRadialLineOfSightTest::RunTest(const FString& Parameters)
{
	// An explosion at (0, 0, 50) with a 400 cm radius: the character in the open takes the falloff at its capsule's
	// centre, the one behind a wall nothing (the Visibility line is blocked), the one out of reach nothing, and the
	// causer itself nothing; without the line of sight test (ECC_MAX) the wall does not shield.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	ACharacter* Open = SpawnStandingCharacter(World, FVector(200.0f, 0.0f, 0.0f));
	ACharacter* Covered = SpawnStandingCharacter(World, FVector(0.0f, 300.0f, 0.0f));
	ACharacter* Far = SpawnStandingCharacter(World, FVector(1000.0f, 0.0f, 0.0f));
	ACharacter* Causer = SpawnStandingCharacter(World, FVector(-150.0f, 0.0f, 0.0f));
	(void)SpawnWall(World, FVector(0.0f, 150.0f, 100.0f), FVector(3.0f, 0.2f, 3.0f));
	FDamageLog OpenLog;
	FDamageLog CoveredLog;
	FDamageLog FarLog;
	FDamageLog CauserLog;
	OpenLog.Watch(*Open);
	CoveredLog.Watch(*Covered);
	FarLog.Watch(*Far);
	CauserLog.Watch(*Causer);

	const FVector Origin(0.0f, 0.0f, 50.0f);
	const bool bApplied = UGameplayStatics::ApplyRadialDamageWithFalloff(
		&World, 100.0f, 0.0f, Origin, 0.0f, 400.0f, 1.0f, nullptr, TArray<AActor*>(), Causer, nullptr);
	TestTrue("Applied", bApplied);
	const float CentreDistance = FVector::Dist(Origin, FVector(200.0f, 0.0f, 92.5f));
	TestEqual(
		"The falloff at the capsule's centre", OpenLog.AnyDamage, 100.0f * (1.0f - (CentreDistance / 400.0f)), 1.0e-3f);
	TestEqual("A radial event", OpenLog.RadialCount, 1);
	TestTrue("Its origin", OpenLog.Origin == Origin);
	TestEqual("Behind the wall", CoveredLog.AnyCount, 0);
	TestEqual("Out of reach", FarLog.AnyCount, 0);
	TestEqual("The causer", CauserLog.AnyCount, 0);

	// No line of sight test, full damage (ApplyRadialDamage with bDoFullDamage), the causer an ignored actor now.
	TArray<AActor*> Ignore;
	Ignore.Add(Open);
	TestTrue("Applied again",
		UGameplayStatics::ApplyRadialDamage(&World, 50.0f, Origin, 400.0f, nullptr, Ignore, nullptr, nullptr,
			/*bDoFullDamage=*/true, ECC_MAX));
	TestEqual("Through the wall", CoveredLog.AnyDamage, 50.0f, 1.0e-4f);
	TestEqual("The causer this time", CauserLog.AnyDamage, 50.0f, 1.0e-4f);
	TestEqual("Ignored", OpenLog.AnyCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectileMovementFlightTest, "System.Engine.ProjectileMovement.Flight",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FProjectileMovementFlightTest::RunTest(const FString& Parameters)
{
	// InitialSpeed along the actor's facing (local space), then gravity: the exact parabola of UE's Verlet steps.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	TestEqual("UE's gravity", World.GetGravityZ(), -980.0f);
	AEngineTestProjectile* Projectile =
		World.SpawnActor<AEngineTestProjectile>(FVector(0.0f, 0.0f, 1000.0f), FRotator(0.0f, 90.0f, 0.0f));
	UProjectileMovementComponent* Movement = Projectile->MovementComp;
	TestTrue("Along the facing", Movement->Velocity.Equals(FVector(0.0f, 1000.0f, 0.0f), 1.0e-2f));
	TickFrames(World, 30);
	const FVector Location = Projectile->GetActorLocation();
	TestEqual("Across", Location.Y, 500.0f, 0.05f);
	TestEqual("Fallen", Location.Z, 1000.0f - (0.5f * 980.0f * 0.25f), 0.05f);
	TestEqual("Falling", Movement->Velocity.Z, -490.0f, 0.05f);

	Movement->ProjectileGravityScale = 0.0f;
	Movement->MaxSpeed = 100.0f;
	TestTrue("MaxSpeed",
		Movement->ComputeVelocity(FVector(0.0f, 1000.0f, 0.0f), 0.1f).Equals(FVector(0.0f, 100.0f, 0.0f), 1.0e-3f));
	TestFalse("Still flying", Movement->HasStoppedSimulation());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectileMovementBounceAndStopTest, "System.Engine.ProjectileMovement.BounceAndStop",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FProjectileMovementBounceAndStopTest::RunTest(const FString& Parameters)
{
	// A bouncing projectile thrown down at 45 degrees onto a floor: the bounce keeps Bounciness of the normal speed and
	// loses Friction of the tangential one, then it bounces to a stop on the floor. Without bShouldBounce a wall stops
	// it at once.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	(void)SpawnWall(World, FVector(0.0f, 0.0f, -50.0f), FVector(40.0f, 40.0f, 1.0f));
	AEngineTestProjectile* Grenade =
		World.SpawnActor<AEngineTestProjectile>(FVector(0.0f, 0.0f, 100.0f), FRotator(-45.0f, 0.0f, 0.0f));
	UProjectileMovementComponent* Movement = Grenade->MovementComp;
	int32 Bounces = 0;
	int32 Stops = 0;
	FVector FirstImpactVelocity = FVector::ZeroVector;
	FVector FirstBounceVelocity = FVector::ZeroVector;
	FVector FirstNormal = FVector::ZeroVector;
	Movement->OnProjectileBounce.AddLambda(
		[&](const FHitResult& Hit, const FVector& ImpactVelocity)
		{
			if (Bounces++ == 0)
			{
				FirstImpactVelocity = ImpactVelocity;
				FirstBounceVelocity = Movement->Velocity;
				FirstNormal = Hit.ImpactNormal;
			}
		});
	Movement->OnProjectileStop.AddLambda([&](const FHitResult&) { ++Stops; });
	TickFrames(World, 20);
	if (!TestTrue("Bounced", Bounces > 0))
	{
		return false;
	}
	TestTrue("Off the floor", FirstNormal.Equals(FVector(0.0f, 0.0f, 1.0f), 1.0e-4f));
	TestTrue("Was going down", FirstImpactVelocity.Z < -600.0f);
	TestEqual("Restitution", FirstBounceVelocity.Z, -0.6f * FirstImpactVelocity.Z, 0.01f);
	TestEqual("Friction", FirstBounceVelocity.X, 0.8f * FirstImpactVelocity.X, 0.01f);
	TickFrames(World, 600);
	TestEqual("Stopped once", Stops, 1);
	TestTrue("Stopped", Movement->HasStoppedSimulation() && Movement->Velocity.IsZero());
	TestEqual("Resting on the floor", Grenade->GetActorLocation().Z, 5.0f, 0.5f);

	AEngineTestProjectile* Bullet =
		World.SpawnActor<AEngineTestProjectile>(FVector(-500.0f, 0.0f, 100.0f), FRotator::ZeroRotator);
	(void)SpawnWall(World, FVector(0.0f, 0.0f, 100.0f), FVector(0.2f, 4.0f, 4.0f));
	Bullet->MovementComp->bShouldBounce = false;
	Bullet->MovementComp->ProjectileGravityScale = 0.0f;
	int32 BulletStops = 0;
	FVector StopPoint = FVector::ZeroVector;
	Bullet->MovementComp->OnProjectileStop.AddLambda(
		[&](const FHitResult& Hit)
		{
			++BulletStops;
			StopPoint = Hit.ImpactPoint;
		});
	TickFrames(World, 60);
	TestEqual("The wall stopped it", BulletStops, 1);
	TestEqual("On the wall's face", StopPoint.X, -10.0f, 0.5f);
	TestEqual("Just short of it", Bullet->GetActorLocation().X, -15.0f, 0.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpectatorPawnStateTest, "System.Engine.SpectatorPawn.PlayerControllerSpectates",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FSpectatorPawnStateTest::RunTest(const FString& Parameters)
{
	// A player's controller plays with its pawn; ChangeState(NAME_Spectating) lets it go and flies a spectator pawn
	// (the game mode's SpectatorClass) from the pawn's eyes; possessing a pawn again ends the spectating.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	APlayerController* Controller = World.SpawnActor<APlayerController>();
	ADefaultPawn* Pawn = World.SpawnActor<ADefaultPawn>(FVector(100.0f, 200.0f, 300.0f), FRotator::ZeroRotator);
	Controller->Possess(Pawn);
	TestTrue("Playing", Controller->IsInState(NAME_Playing));

	Controller->ChangeState(NAME_Spectating);
	ASpectatorPawn* Spectator = Controller->GetSpectatorPawn();
	if (!TestNotNull("A spectator pawn", Spectator))
	{
		return false;
	}
	TestTrue("Spectating", Controller->GetStateName() == NAME_Spectating);
	TestTrue("It flies the spectator", Controller->GetPawn() == Spectator);
	TestNull("The old pawn is let go", Pawn->GetController());
	TestTrue("From the pawn's eyes", Spectator->GetActorLocation().Equals(FVector(100.0f, 200.0f, 300.0f), 1.0e-3f));
	TestFalse("Never damaged", Spectator->CanBeDamaged());
	TestNotNull("Spectator movement", Spectator->GetSpectatorPawnMovement());

	ADefaultPawn* Respawned = World.SpawnActor<ADefaultPawn>();
	Controller->Possess(Respawned);
	TestTrue("Playing again", Controller->IsInState(NAME_Playing));
	TestNull("No spectator", Controller->GetSpectatorPawn());
	TestTrue("The spectator is gone", Spectator->IsPendingKillPending());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStaticMeshSocketTransformTest, "System.Engine.Sockets.StaticMeshSocketTransform",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FStaticMeshSocketTransformTest::RunTest(const FString& Parameters)
{
	// A static mesh socket in the world is its transform in the mesh times the component's; a component attached to
	// the socket follows it; an unknown socket is the component itself.
	UStaticMesh* Mesh = NewObject<UStaticMesh>();
	UStaticMeshSocket* Muzzle = NewObject<UStaticMeshSocket>(Mesh, TEXT("StaticMeshSocket_Muzzle"));
	Muzzle->SocketName = TEXT("Muzzle");
	Muzzle->RelativeLocation = FVector(50.0f, 0.0f, 10.0f);
	Muzzle->RelativeRotation = FRotator(0.0f, 0.0f, 0.0f);
	Mesh->Sockets.Add(Muzzle);
	TestTrue("FindSocket", Mesh->FindSocket(TEXT("Muzzle")) == Muzzle);
	TestNull("No such socket", Mesh->FindSocket(TEXT("Grip")));

	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AStaticMeshActor* Weapon =
		World.SpawnActor<AStaticMeshActor>(FVector(100.0f, 0.0f, 0.0f), FRotator(0.0f, 90.0f, 0.0f));
	UStaticMeshComponent* MeshComp = Weapon->GetStaticMeshComponent();
	(void)MeshComp->SetStaticMesh(Mesh);
	TestTrue("DoesSocketExist", MeshComp->DoesSocketExist(TEXT("Muzzle")));
	TestFalse("Not a socket", MeshComp->DoesSocketExist(TEXT("Grip")));
	const FTransform Socket = MeshComp->GetSocketTransform(TEXT("Muzzle"));
	TestTrue("Turned with the component", Socket.GetLocation().Equals(FVector(100.0f, 50.0f, 10.0f), 1.0e-3f));
	TestEqual("Its yaw", Socket.Rotator().Yaw, 90.0f, 1.0e-3f);
	TestTrue("A missing socket is the component",
		MeshComp->GetSocketTransform(TEXT("Grip")).GetLocation().Equals(FVector(100.0f, 0.0f, 0.0f), 1.0e-3f));

	USceneComponent* Flash = NewObject<USceneComponent>(Weapon, TEXT("Flash"));
	Flash->RegisterComponent();
	TestTrue("Attached to the socket",
		Flash->AttachToComponent(MeshComp, FAttachmentTransformRules::KeepRelativeTransform, TEXT("Muzzle")));
	TestTrue("Follows it", Flash->GetComponentLocation().Equals(FVector(100.0f, 50.0f, 10.0f), 1.0e-3f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActorLifeSpanTest, "System.Engine.Actor.LifeSpan",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FActorLifeSpanTest::RunTest(const FString& Parameters)
{
	// SetLifeSpan destroys the actor after that much world time; 0 cancels; the world counts its time.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AActor* Short = World.SpawnActor<AActor>();
	AActor* Cancelled = World.SpawnActor<AActor>();
	Short->SetLifeSpan(0.5f);
	Cancelled->SetLifeSpan(0.25f);
	Cancelled->SetLifeSpan(0.0f);
	TestEqual("Remaining", Short->GetLifeSpan(), 0.5f);
	TickFrames(World, 20);
	TestFalse("Not yet", Short->IsPendingKillPending());
	TickFrames(World, 11);
	TestTrue("Expired", Short->IsPendingKillPending());
	TestFalse("Cancelled", Cancelled->IsPendingKillPending());
	TestEqual("World time", World.GetTimeSeconds(), 31.0f / 60.0f, 1.0e-4f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
