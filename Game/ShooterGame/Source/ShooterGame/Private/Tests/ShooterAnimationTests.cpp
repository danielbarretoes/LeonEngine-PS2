#include "Animation/AimOffsetBlendSpace1D.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpaceBase.h"
#include "Animation/CharacterAnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CoreMinimal.h"
#include "Engine/BlockingVolume.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "ShooterAIController.h"
#include "ShooterCharacter.h"
#include "ShooterPlayerState.h"
#include "Tests/ScopedTestWorld.h"
#include "Tests/SkinnedTestMesh.h"
#include "Weapons/ShooterWeapon.h"

#if WITH_DEV_AUTOMATION_TESTS

// The pawn's animation paths (Docs/PLANS/ps2-shipping.md N25, N27): ShooterGame's characters, arms and weapons as the
// config names them (N27's art), and a test character built from the engine's skinned test fixture
// (FSkinnedTestCharacter) as the body and the first-person arms, with test montages.

namespace
{

	constexpr float FrameTime = 1.0f / 30.0f;

	void TickFrames(UWorld& World, int32 Frames)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			World.Tick(FrameTime);
		}
	}

	/** A bot pawn of the CT on a floor at the origin, its brain off. */
	AShooterCharacter* SpawnPawn(UWorld& World)
	{
		(void)World.SpawnActor<ABlockingVolume>(ABlockingVolume::StaticClass(),
			FTransform(FQuat::Identity, FVector(0.0f, 0.0f, -50.0f), FVector(80.0f, 80.0f, 1.0f)));
		AShooterCharacter* Pawn = World.SpawnActor<AShooterCharacter>(FVector::ZeroVector, FRotator(0.0f, 0.0f, 0.0f));
		AShooterAIController* Controller = World.SpawnActor<AShooterAIController>();
		Controller->SetActorTickEnabled(false);
		if (AShooterPlayerState* State = Controller->GetPlayerState<AShooterPlayerState>())
		{
			State->SetTeam(EShooterTeam::CT);
		}
		Controller->Possess(Pawn);
		return Pawn;
	}

	/** An upper-body montage of Length seconds that moves the test character's hand, with a notify. */
	UAnimMontage* MakeWeaponMontage(float Length, FName NotifyName, float NotifyTime)
	{
		UAnimSequence* Clip =
			FSkinnedTestCharacter::MakeOffsetClip(FSkinnedTestCharacter::HandR, FVector(0.0f, 0.0f, 5.0f), Length);
		Clip->bLoop = false;
		Clip->AddNotify(NotifyName, NotifyTime);
		UAnimMontage* Montage = NewObject<UAnimMontage>();
		Montage->SetAnimation(Clip);
		Montage->SlotName = FName(TEXT("UpperBody"));
		return Montage;
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameSkinnedPawnTest, "ShooterGame.Animation.SkinnedPawn",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShooterGameSkinnedPawnTest::RunTest(const FString& Parameters)
{
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterCharacter* Pawn = SpawnPawn(World);
	AShooterWeapon* Rifle = Pawn->GiveWeapon(AShooterWeapon::FindWeaponClass(TEXT("ak47")));
	if (!TestNotNull("A rifle", Rifle))
	{
		return false;
	}
	Pawn->EquipWeapon(Rifle);
	TickFrames(World, 3);

	// Without a body or arms, the weapon sits at its offsets from the capsule and the camera.
	Pawn->SetSkeletalBody(nullptr);
	Pawn->SetArmsMesh(nullptr);
	TestFalse("No skinned body", Pawn->HasSkeletalBody());
	TestTrue("3P on the capsule",
		Rifle->GetMesh3P()->GetAttachParent() == Pawn->GetRootComponent() &&
			Rifle->GetMesh3P()->GetAttachSocketName().IsNone());
	TestTrue("1P on the camera", Rifle->GetMesh1P()->GetAttachParent() == Pawn->GetFirstPersonCameraComponent());

	// The test character as the body and the arms: the weapon moves to their Weapon_R sockets.
	USkeletalMesh* Body = FSkinnedTestCharacter::MakeMesh();
	USkeletalMesh* Arms = FSkinnedTestCharacter::MakeMesh();
	if (!TestTrue("The test meshes", Body != nullptr && Arms != nullptr))
	{
		return false;
	}
	Pawn->SetSkeletalBody(Body);
	Pawn->SetArmsMesh(Arms);
	TestTrue("Skinned body and arms", Pawn->HasSkeletalBody() && Pawn->HasArms());
	TestTrue("The arms: a view model for their player only",
		Pawn->GetMesh1P()->bRenderAsViewModel && Pawn->GetMesh1P()->bOnlyOwnerSee && Pawn->GetMesh().bOwnerNoSee);
	TestTrue("3P on the body's hand socket",
		Rifle->GetMesh3P()->GetAttachParent() == &Pawn->GetMesh() &&
			Rifle->GetMesh3P()->GetAttachSocketName() == FName(TEXT("Weapon_R")));
	TestTrue("1P on the arms' hand socket",
		Rifle->GetMesh1P()->GetAttachParent() == Pawn->GetMesh1P() &&
			Rifle->GetMesh1P()->GetAttachSocketName() == FName(TEXT("Weapon_R")));
	TickFrames(World, 2);
	TestTrue("The weapon where the hand's socket is",
		Rifle->GetMesh3P()->GetComponentLocation().Equals(
			Pawn->GetMesh().GetSocketTransform(TEXT("Weapon_R")).GetLocation(), 0.5f));

	// The weapon's montages play on both meshes; a reload lasts CS's ReloadDuration, its 0.6 s montage played to fit
	// it (the weapons of a kind share their clips), and the body's notify reaches the pawn.
	UAnimMontage* Reload1P = MakeWeaponMontage(0.6f, TEXT("MagOut"), 0.2f);
	UAnimMontage* Reload3P = MakeWeaponMontage(0.6f, TEXT("MagOut"), 0.2f);
	Rifle->ReloadAnim = {Reload1P, Reload3P};
	Rifle->FireAnim = {MakeWeaponMontage(0.1f, TEXT("Fire"), 0.0f), MakeWeaponMontage(0.1f, TEXT("Fire"), 0.0f)};
	int32 MagOut = 0;
	Pawn->GetMesh().GetAnimInstance().OnAnimNotify.AddLambda(
		[&MagOut](FName Name, const UAnimSequenceBase*) { MagOut += Name == FName(TEXT("MagOut")) ? 1 : 0; });
	TickFrames(World, FMath::CeilToInt((Rifle->EquipDuration + 0.1f) / FrameTime));
	Rifle->SetAmmo(5, 30);
	Pawn->ReloadWeapon();
	TestTrue("Reloading", Rifle->GetCurrentState() == EShooterWeaponState::Reloading);
	TestTrue("The montages play on the arms and the body",
		Pawn->GetMesh1P()->GetAnimInstance().Montage_IsPlaying(Reload1P) &&
			Pawn->GetMesh().GetAnimInstance().Montage_IsPlaying(Reload3P));
	TickFrames(World, 21);
	TestTrue("Still reloading after the clip's 0.6 s",
		Rifle->GetCurrentState() == EShooterWeaponState::Reloading &&
			Pawn->GetMesh().GetAnimInstance().Montage_IsPlaying(Reload3P));
	TickFrames(World, FMath::CeilToInt((Rifle->ReloadDuration - (21.0f * FrameTime)) / FrameTime) + 1);
	TestTrue("Done after ReloadDuration",
		Rifle->GetCurrentState() == EShooterWeaponState::Idle && Rifle->GetCurrentAmmoInClip() == Rifle->AmmoPerClip &&
			Rifle->ReloadDuration > 1.0f);
	TestEqual("Its MagOut notify, once", MagOut, 1);

	Pawn->StartWeaponFire();
	TestTrue("Firing plays the fire montage on the body",
		Pawn->GetMesh().GetAnimInstance().GetCurrentActiveMontage() == Rifle->FireAnim.Pawn3P);
	Pawn->StopWeaponFire();

	// Without the body: the weapon returns to its offset on the capsule.
	Pawn->SetSkeletalBody(nullptr);
	TestTrue(
		"No body again", !Pawn->HasSkeletalBody() && Rifle->GetMesh3P()->GetAttachParent() == Pawn->GetRootComponent());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterGameCharacterArtTest, "ShooterGame.Animation.CharacterArt",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShooterGameCharacterArtTest::RunTest(const FString& Parameters)
{
	// N27's art as the config names it: the CT's body and arms, the USP's two models on the Weapon_R sockets, its arms'
	// idle and its stance, draw and reload montages as long as CS's times, the crouch, and a death that holds.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	AShooterCharacter* Pawn = SpawnPawn(World);
	// The team's meshes as UpdateBody takes them (a test world has no game mode to give the bot a team).
	Pawn->SetSkeletalBody(Cast<USkeletalMesh>(Pawn->CTBodyMeshName.TryLoad()));
	Pawn->SetArmsMesh(Cast<USkeletalMesh>(Pawn->CTArmsMeshName.TryLoad()));
	TickFrames(World, 2);
	const USkeletalMesh* Body = Pawn->GetMesh().GetSkeletalMesh();
	const USkeletalMesh* Arms = Pawn->GetMesh1P()->GetSkeletalMesh();
	if (!TestTrue("The CT's body and arms",
			Body != nullptr && Body->GetName() == TEXT("SK_Body_CT") && Arms != nullptr &&
				Arms->GetName() == TEXT("SK_Arms_CT")))
	{
		return false;
	}
	TestTrue("The Weapon_R sockets",
		Pawn->GetMesh().DoesSocketExist(TEXT("Weapon_R")) && Pawn->GetMesh1P()->DoesSocketExist(TEXT("Weapon_R")));
	AShooterWeapon* Usp = Pawn->GetWeapon();
	if (!TestTrue("The default pistol drawn", Usp != nullptr && Usp->WeaponName == TEXT("usp")))
	{
		return false;
	}
	TestTrue("Its world and view models",
		Usp->GetMesh3P()->GetStaticMesh() != nullptr &&
			Usp->GetMesh3P()->GetStaticMesh()->GetName() == TEXT("SM_USP") &&
			Usp->GetMesh1P()->GetStaticMesh() != nullptr &&
			Usp->GetMesh1P()->GetStaticMesh()->GetName() == TEXT("SM_USP_1P"));
	TestTrue("On the hands' sockets",
		Usp->GetMesh3P()->GetAttachSocketName() == FName(TEXT("Weapon_R")) &&
			Usp->GetMesh1P()->GetAttachSocketName() == FName(TEXT("Weapon_R")));
	TestTrue("The arms idle with the pistol",
		Usp->GetArmsIdle() != nullptr && Usp->GetArmsIdle()->GetName() == TEXT("BS_Pistol_Idle"));
	// The arms hold the pistol where its idle puts it (anim_arms.py: the grip 38 cm ahead of the eye, 14 cm right, 21
	// cm down; the reference pose has it at 48, 7 and 17), the renderer's stamp keeping the pose evaluated.
	for (int32 Frame = 0; Frame < 45; ++Frame)
	{
		Pawn->GetMesh1P()->LastRenderTime = World.GetTimeSeconds();
		World.Tick(FrameTime);
	}
	const FVector Grip = Pawn->GetMesh1P()->GetSocketTransform(TEXT("Weapon_R")).GetLocation() -
		Pawn->GetFirstPersonCameraComponent()->GetComponentLocation();
	TestTrue(*FString::Printf("The arms hold the pistol (%s)", *Grip.ToString()),
		Grip.Equals(FVector(38.0f, 14.0f, -21.0f), 1.5f));
	TestTrue(
		"The pistol's stance", Usp->GetAimOffset() != nullptr && Usp->GetAimOffset()->GetName() == TEXT("AO_Pistol"));
	TestTrue("A draw as long as CS's (1 s)",
		Usp->EquipAnim.Pawn1P != nullptr &&
			FMath::IsNearlyEqual(Usp->EquipAnim.Pawn1P->GetPlayLength(), 1.0f, 1.0e-3f));
	TestTrue("A reload as long as CS's (2.7 s), on both",
		Usp->ReloadAnim.Pawn1P != nullptr && Usp->ReloadAnim.Pawn3P != nullptr &&
			FMath::IsNearlyEqual(Usp->ReloadAnim.Pawn1P->GetPlayLength(), 2.7f, 1.0e-3f) &&
			FMath::IsNearlyEqual(Usp->ReloadAnim.Pawn3P->GetPlayLength(), 2.7f, 1.0e-3f));

	// Crouching crossfades the body into the crouched locomotion.
	Pawn->Crouch();
	TickFrames(World, 10);
	const UCharacterAnimInstance* Anim = Cast<UCharacterAnimInstance>(&Pawn->GetMesh().GetAnimInstance());
	TestTrue("Crouched", Anim != nullptr && Anim->IsCrouched() && Anim->GetCrouchAlpha() == 1.0f);
	Pawn->UnCrouch();
	TickFrames(World, 1);

	// Death: a death montage lays the body down and still holds it three seconds later.
	Pawn->Suicide();
	const UAnimMontage* Death = Pawn->GetMesh().GetAnimInstance().GetCurrentActiveMontage();
	TestTrue("A death montage", Death != nullptr && Death->GetName().StartsWith(TEXT("AM_Death_")));
	TickFrames(World, 90);
	TestTrue("It holds the body down", Pawn->GetMesh().GetAnimInstance().GetCurrentActiveMontage() == Death);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
