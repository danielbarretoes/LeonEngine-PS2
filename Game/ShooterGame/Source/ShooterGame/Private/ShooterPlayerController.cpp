#include "ShooterPlayerController.h"

#include "AudioDevice.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/ForceFeedbackEffect.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/SpectatorPawn.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ShooterCharacter.h"
#include "ShooterGame.h"
#include "ShooterGameMode.h"
#include "ShooterGameState.h"
#include "ShooterHUD.h"
#include "ShooterPersistentUser.h"
#include "ShooterPlayerState.h"
#include "Sound/SoundWave.h"
#include "TimerManager.h"
#include "Weapons/ShooterWeapon.h"
#include "Weapons/ShooterWeapon_Projectile.h"

namespace
{

	/** A page of the buy menu: a category of items, or (DirectItem) a first-page line that buys at once. */
	struct FBuyMenuCategory
	{
		const TCHAR* Label;
		TArrayView<const TCHAR* const> Items;
		const TCHAR* DirectItem;
	};

	/** CS 1.6's buy menu, with ShooterGame's weapons (buy names, AShooterGameMode::Buy). */
	const TCHAR* const PistolItems[] = {TEXT("glock"), TEXT("usp"), TEXT("deagle")};
	const TCHAR* const SmgItems[] = {TEXT("mp5")};
	const TCHAR* const RifleItems[] = {TEXT("ak47"), TEXT("m4a1"), TEXT("awp")};
	const TCHAR* const EquipmentItems[] = {
		TEXT("vest"), TEXT("vesthelm"), TEXT("flashbang"), TEXT("hegrenade"), TEXT("smokegrenade"), TEXT("defuser")};

	const FBuyMenuCategory BuyMenuCategories[] = {
		{TEXT("Pistols"), PistolItems, nullptr},
		{TEXT("SMGs"), SmgItems, nullptr},
		{TEXT("Rifles"), RifleItems, nullptr},
		{TEXT("Primary ammo"), {}, TEXT("primammo")},
		{TEXT("Secondary ammo"), {}, TEXT("secammo")},
		{TEXT("Equipment"), EquipmentItems, nullptr},
	};

	/** An item on a category's page for a player of Team: a weapon only when the team may buy it (CS). */
	bool IsListedFor(const TCHAR* Item, EShooterTeam Team)
	{
		const UClass* WeaponClass = AShooterWeapon::FindWeaponClass(Item);
		return WeaponClass == nullptr || Team == EShooterTeam::None ||
			WeaponClass->GetDefaultObject<AShooterWeapon>()->CanBeBoughtBy(Team);
	}

} // namespace

namespace
{
	/** A force feedback effect: one line from Start to End over Duration on the large or the small motors. */
	void SetEffect(UForceFeedbackEffect* Effect, float Duration, bool bLarge, bool bSmall, float Start, float End)
	{
		FForceFeedbackChannelDetails Channel;
		Channel.bAffectsLeftLarge = bLarge;
		Channel.bAffectsRightLarge = bLarge;
		Channel.bAffectsLeftSmall = bSmall;
		Channel.bAffectsRightSmall = bSmall;
		Channel.StartIntensity = Start;
		Channel.EndIntensity = End;
		Effect->Duration = Duration;
		Effect->ChannelDetails.Reset();
		Effect->ChannelDetails.Add(Channel);
	}

	/** Whether the player plays at a screen (the options are theirs): not a headless run nor a test's engine. */
	bool HasScreen()
	{
		return GEngine != nullptr && GEngine->GameViewport != nullptr && GEngine->GameViewport->GetWindow() != nullptr;
	}
} // namespace

AShooterPlayerController::AShooterPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// UE ShooterGame's effects: a short buzz of the small motor a shot, the large one when hurt and near an explosion.
	FireForceFeedback = CreateDefaultSubobject<UForceFeedbackEffect>(TEXT("FireForceFeedback"));
	SetEffect(FireForceFeedback, 0.1f, false, true, 1.0f, 1.0f);
	HitForceFeedback = CreateDefaultSubobject<UForceFeedbackEffect>(TEXT("HitForceFeedback"));
	SetEffect(HitForceFeedback, 0.25f, true, false, 0.8f, 0.0f);
	ExplosionForceFeedback = CreateDefaultSubobject<UForceFeedbackEffect>(TEXT("ExplosionForceFeedback"));
	SetEffect(ExplosionForceFeedback, 0.6f, true, true, 1.0f, 0.0f);
}

void AShooterPlayerController::BeginPlay()
{
	Super::BeginPlay();
	bFollowPlayers = IsLocalController() && FParse::Param(FCommandLine::Get(), TEXT("BotMatchSpectate"));
	RadioSounds = {LoadShooterSound(RadioCommandSoundName), LoadShooterSound(RadioGroupSoundName),
		LoadShooterSound(RadioReportSoundName), LoadShooterSound(RadioFireInTheHoleSoundName),
		LoadShooterSound(RadioBombPlantedSoundName)};
	// The options of a player at a screen: from the save slot (the memory card on the PS2).
	if (IsLocalController() && HasScreen())
	{
		const ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(Player);
		PersistentUser =
			UShooterPersistentUser::LoadPersistentUser(LocalPlayer != nullptr ? LocalPlayer->GetControllerId() : 0);
		ApplyPersistentUser();
	}
}

USoundWave* AShooterPlayerController::GetRadioSound(EShooterRadioMessage Message) const
{
	if (RadioSounds.Num() != 5)
	{
		return nullptr;
	}
	switch (Message)
	{
		case EShooterRadioMessage::None:
			return nullptr;
		case EShooterRadioMessage::FireInTheHole:
			return RadioSounds[3];
		case EShooterRadioMessage::BombPlanted:
			return RadioSounds[4];
		default:
			break;
	}
	const int32 Menu = GetRadioMessageMenu(Message);
	return Menu >= 1 && Menu <= NumRadioMenus ? RadioSounds[Menu - 1] : nullptr;
}

void AShooterPlayerController::HearRadio(const FShooterRadioEntry& Entry)
{
	LastRadioSound = GetRadioSound(Entry.Message);
	if (LastRadioSound != nullptr)
	{
		UGameplayStatics::PlaySound2D(this, LastRadioSound);
	}
}

UShooterPersistentUser* AShooterPlayerController::GetPersistentUser()
{
	if (PersistentUser == nullptr)
	{
		PersistentUser =
			Cast<UShooterPersistentUser>(UGameplayStatics::CreateSaveGameObject(UShooterPersistentUser::StaticClass()));
	}
	return PersistentUser;
}

float AShooterPlayerController::GetAimSensitivity() const
{
	return PersistentUser != nullptr ? FMath::Max(PersistentUser->AimSensitivity, 0.01f) : 1.0f;
}

void AShooterPlayerController::ApplyPersistentUser()
{
	const UShooterPersistentUser* User = GetPersistentUser();
	if (PlayerInput != nullptr)
	{
		if (BaseMouseSensitivity < 0.0f)
		{
			BaseMouseSensitivity = PlayerInput->GetMouseSensitivityX();
		}
		PlayerInput->SetMouseSensitivity(BaseMouseSensitivity * GetAimSensitivity());
		// Up looks down: the pitch axes' mappings turn their sign (UE ShooterGame's inverted axis).
		bool bChanged = false;
		for (FInputAxisKeyMapping& Mapping : PlayerInput->AxisMappings)
		{
			if (Mapping.AxisName == TEXT("LookUp") || Mapping.AxisName == TEXT("LookUpRate"))
			{
				const float Scale = User->bInvertedYAxis ? -FMath::Abs(Mapping.Scale) : FMath::Abs(Mapping.Scale);
				bChanged |= Scale != Mapping.Scale;
				Mapping.Scale = Scale;
			}
		}
		if (bChanged)
		{
			PlayerInput->ForceRebuildingKeyMaps(false);
		}
	}
	if (GEngine != nullptr)
	{
		GEngine->GetAudioDevice().SetMasterVolume(User->SoundVolume);
	}
	if (AShooterHUD* ShooterHUD = Cast<AShooterHUD>(MyHUD))
	{
		ShooterHUD->CrosshairColor = User->CrosshairColor;
	}
}

void AShooterPlayerController::SavePersistentUser()
{
	ApplyPersistentUser();
	if (HasScreen())
	{
		const ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(Player);
		(void)GetPersistentUser()->SaveToSlot(LocalPlayer != nullptr ? LocalPlayer->GetControllerId() : 0);
	}
}

void AShooterPlayerController::SetSensitivity(float Scale)
{
	GetPersistentUser()->AimSensitivity = FMath::Clamp(Scale, 0.1f, 10.0f);
	SavePersistentUser();
}

void AShooterPlayerController::SetInvertY(int32 Invert)
{
	GetPersistentUser()->bInvertedYAxis = Invert != 0;
	SavePersistentUser();
}

void AShooterPlayerController::SetVolume(float Volume)
{
	GetPersistentUser()->SoundVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
	SavePersistentUser();
}

void AShooterPlayerController::SetCrosshairColor(float Red, float Green, float Blue)
{
	GetPersistentUser()->CrosshairColor =
		FLinearColor(FMath::Clamp(Red, 0.0f, 1.0f), FMath::Clamp(Green, 0.0f, 1.0f), FMath::Clamp(Blue, 0.0f, 1.0f));
	SavePersistentUser();
}

void AShooterPlayerController::PlayFireForceFeedback()
{
	if (IsLocalController())
	{
		FForceFeedbackParameters Params;
		Params.Tag = TEXT("Weapon");
		ClientPlayForceFeedback(FireForceFeedback, Params);
	}
}

void AShooterPlayerController::PlayHitForceFeedback()
{
	if (IsLocalController())
	{
		FForceFeedbackParameters Params;
		Params.Tag = TEXT("Damage");
		ClientPlayForceFeedback(HitForceFeedback, Params);
	}
}

void AShooterPlayerController::PlayExplosionForceFeedback(UWorld* World, const FVector& Origin, float Radius)
{
	if (World == nullptr)
	{
		return;
	}
	const UGameInstance* GameInstance = World->GetGameInstance();
	if (GameInstance == nullptr)
	{
		return;
	}
	for (const ULocalPlayer* LocalPlayer : GameInstance->GetLocalPlayers())
	{
		AShooterPlayerController* Controller =
			LocalPlayer != nullptr ? Cast<AShooterPlayerController>(LocalPlayer->PlayerController) : nullptr;
		if (Controller == nullptr || Controller->GetWorld() != World)
		{
			continue;
		}
		FVector ViewLocation;
		FRotator ViewRotation;
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
		if (FVector::DistSquared(ViewLocation, Origin) <= FMath::Square(Radius * 2.0f))
		{
			FForceFeedbackParameters Params;
			Params.Tag = TEXT("Explosion");
			Controller->ClientPlayForceFeedback(Controller->ExplosionForceFeedback, Params);
		}
	}
}

void AShooterPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (!IsInState(NAME_Spectating))
	{
		return;
	}
	// Only the free look flies: the death cam holds at the corpse, and while a player is watched the spectator waits
	// where the view will come back to.
	ASpectatorPawn* Spectator = GetSpectatorPawn();
	if (Spectator != nullptr && SpectatorMode != EShooterSpectatorMode::FreeLook)
	{
		(void)Spectator->ConsumeMovementInputVector();
	}
	if (SpectatorMode == EShooterSpectatorMode::DeathCam)
	{
		UpdateDeathCam();
	}
	// The watched player died: the next one (-BotMatchSpectate: always somebody when anybody lives).
	else if ((bFollowPlayers || SpectatorMode == EShooterSpectatorMode::Player) && GetViewedPlayer() == nullptr)
	{
		ViewPlayer(1);
	}
}

void AShooterPlayerController::BeginSpectatingState()
{
	Super::BeginSpectatingState();
	SpectatorMode = EShooterSpectatorMode::FreeLook;
	SetSpectatorInput(true);
}

void AShooterPlayerController::EndSpectatingState()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle_DeathCam);
	}
	DeathCamTarget.Reset();
	DeathCamKillerName.Empty();
	SpectatorMode = EShooterSpectatorMode::None;
	SetSpectatorInput(false);
	Super::EndSpectatingState();
}

void AShooterPlayerController::SetSpectatorInput(bool bEnabled)
{
	if (SpectatorInputComponent == nullptr || bEnabled == bSpectatorInputPushed)
	{
		return;
	}
	if (bEnabled)
	{
		PushInputComponent(SpectatorInputComponent);
	}
	else
	{
		(void)PopInputComponent(SpectatorInputComponent);
	}
	bSpectatorInputPushed = bEnabled;
}

void AShooterPlayerController::StartDeathCam(AActor* Killer)
{
	if (!IsInState(NAME_Spectating))
	{
		ChangeState(NAME_Spectating);
	}
	// The spectator stands where the player looked from (BeginSpectatingState): the corpse's eyes.
	SpectatorMode = EShooterSpectatorMode::DeathCam;
	DeathCamTarget = Killer;
	const APawn* KillerPawn = Cast<APawn>(Killer);
	const AController* KillerController = KillerPawn != nullptr ? KillerPawn->GetController() : nullptr;
	const APlayerState* KillerState =
		KillerController != nullptr ? KillerController->GetPlayerState<APlayerState>() : nullptr;
	DeathCamKillerName = KillerState != nullptr ? KillerState->GetPlayerName() : FString();
	if (PlayerCameraManager != nullptr)
	{
		PlayerCameraManager->SetViewTarget(nullptr);
	}
	UpdateDeathCam();
	GetWorldTimerManager().SetTimer(
		TimerHandle_DeathCam, this, &AShooterPlayerController::OnDeathCamTimer, FMath::Max(DeathCamDuration, 0.01f));
}

void AShooterPlayerController::UpdateDeathCam()
{
	const AActor* Target = DeathCamTarget.Get();
	const APawn* Spectator = GetSpectatorPawn();
	if (Target == nullptr || Target->IsPendingKillPending() || Spectator == nullptr)
	{
		return;
	}
	FVector Eyes;
	FRotator Unused;
	Spectator->GetActorEyesViewPoint(Eyes, Unused);
	FVector TargetEyes = Target->GetActorLocation();
	FRotator TargetRotation;
	if (const APawn* TargetPawn = Cast<APawn>(Target))
	{
		TargetPawn->GetActorEyesViewPoint(TargetEyes, TargetRotation);
	}
	const FVector ToTarget = TargetEyes - Eyes;
	if (!ToTarget.IsNearlyZero())
	{
		SetControlRotation(ToTarget.Rotation());
	}
}

void AShooterPlayerController::OnDeathCamTimer()
{
	if (SpectatorMode == EShooterSpectatorMode::DeathCam)
	{
		ViewPlayer(1);
	}
}

void AShooterPlayerController::BeginFreeLook()
{
	SpectatorMode = EShooterSpectatorMode::FreeLook;
	ASpectatorPawn* Spectator = GetSpectatorPawn();
	if (PlayerCameraManager == nullptr)
	{
		return;
	}
	// From the view the player had (a teammate's eyes, the death cam), not from where the spectator waited.
	if (Spectator != nullptr && PlayerCameraManager->HasCameraCache())
	{
		FVector Eyes;
		FRotator Unused;
		Spectator->GetActorEyesViewPoint(Eyes, Unused);
		const FVector EyeOffset = Eyes - Spectator->GetActorLocation();
		(void)Spectator->SetActorLocation(PlayerCameraManager->GetCameraLocation() - EyeOffset);
		const FRotator View = PlayerCameraManager->GetCameraRotation();
		SetControlRotation(FRotator(View.Pitch, View.Yaw, 0.0f));
	}
	PlayerCameraManager->SetViewTarget(nullptr);
}

void AShooterPlayerController::OnSpectateNext()
{
	ViewPlayer(1);
}

void AShooterPlayerController::OnSpectatePrev()
{
	ViewPlayer(-1);
}

void AShooterPlayerController::OnSpectateToggleFreeLook()
{
	if (SpectatorMode == EShooterSpectatorMode::FreeLook)
	{
		ViewPlayer(1);
	}
	else
	{
		GetWorldTimerManager().ClearTimer(TimerHandle_DeathCam);
		BeginFreeLook();
	}
}

void AShooterPlayerController::NotifyTakeDamage(const FVector& SourceLocation)
{
	const UWorld* World = GetWorld();
	LastDamageTime = World != nullptr ? World->GetTimeSeconds() : 0.0f;
	LastDamageSourceLocation = SourceLocation;
	PlayHitForceFeedback();
}

void AShooterPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// CS closes the buy menu when the buy time ends; here also when the buy zone is left or the round is over: its keys
	// (the numbers, Cross) go back to the pawn.
	FString Reason;
	if (bBuyMenuOpen && !CanOpenBuyMenu(&Reason))
	{
		SetBuyMenuOpen(false);
		OnBuyMenuRefused(Reason);
	}
	// A dead player's radio menu closes (its number keys go back).
	const AShooterCharacter* ShooterPawn = Cast<AShooterCharacter>(GetPawn());
	if (RadioMenu != 0 && (IsInState(NAME_Spectating) || ShooterPawn == nullptr || !ShooterPawn->IsAlive()))
	{
		SetRadioMenu(0);
	}
}

AShooterCharacter* AShooterPlayerController::GetViewedPlayer() const
{
	if (PlayerCameraManager == nullptr || !IsInState(NAME_Spectating))
	{
		return nullptr;
	}
	AShooterCharacter* Viewed = Cast<AShooterCharacter>(PlayerCameraManager->GetViewTarget());
	return Viewed != nullptr && Viewed->IsAlive() && !Viewed->IsPendingKillPending() ? Viewed : nullptr;
}

void AShooterPlayerController::ViewNextPlayer()
{
	ViewPlayer(1);
}

void AShooterPlayerController::ViewPrevPlayer()
{
	ViewPlayer(-1);
}

void AShooterPlayerController::ViewPlayer(int32 Step)
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World != nullptr ? World->GetGameState() : nullptr;
	if (GameState == nullptr || PlayerCameraManager == nullptr || !IsInState(NAME_Spectating))
	{
		return;
	}
	// Watching someone ends the death cam (CS: fire skips it).
	GetWorldTimerManager().ClearTimer(TimerHandle_DeathCam);
	const AShooterPlayerState* OwnState = GetPlayerState<AShooterPlayerState>();
	const EShooterTeam OwnTeam = OwnState != nullptr ? OwnState->GetTeam() : EShooterTeam::None;
	// The living players this spectator may watch, in the game state's order.
	TArray<AShooterCharacter*, TInlineAllocator<16>> Candidates;
	for (const APlayerState* State : GameState->GetPlayerArray())
	{
		const AController* Controller = State != nullptr ? Cast<AController>(State->GetOwner()) : nullptr;
		AShooterCharacter* Shooter = Controller != nullptr ? Cast<AShooterCharacter>(Controller->GetPawn()) : nullptr;
		if (Shooter != nullptr && Shooter->IsAlive() && !Shooter->IsPendingKillPending() &&
			(OwnTeam == EShooterTeam::None || Shooter->GetTeam() == OwnTeam))
		{
			Candidates.Add(Shooter);
		}
	}
	if (Candidates.Num() == 0)
	{
		// Nobody left to watch: the free look (once; it stays until somebody can be watched).
		if (SpectatorMode != EShooterSpectatorMode::FreeLook)
		{
			BeginFreeLook();
		}
		return;
	}
	// The one Step places from the viewed player, wrapping; with none viewed the first (next) or the last (previous).
	const int32 Num = Candidates.Num();
	const int32 Current = Candidates.Find(Cast<AShooterCharacter>(PlayerCameraManager->GetViewTarget()));
	const int32 Index = Current == INDEX_NONE ? (Step >= 0 ? 0 : Num - 1) : (((Current + Step) % Num) + Num) % Num;
	PlayerCameraManager->SetViewTarget(Candidates[Index]);
	SpectatorMode = EShooterSpectatorMode::Player;
}

void AShooterPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindAction(TEXT("Scoreboard"), IE_Pressed, this, &AShooterPlayerController::OnScoreboardPressed);
	InputComponent->BindAction(TEXT("Scoreboard"), IE_Released, this, &AShooterPlayerController::OnScoreboardReleased);
	InputComponent->BindAction(TEXT("BuyMenu"), IE_Pressed, this, &AShooterPlayerController::OnBuyMenuPressed);
	InputComponent->BindAction(TEXT("BuyPrimaryAmmo"), IE_Pressed, this, &AShooterPlayerController::BuyAmmo1);
	InputComponent->BindAction(TEXT("BuySecondaryAmmo"), IE_Pressed, this, &AShooterPlayerController::BuyAmmo2);
	InputComponent->BindAction(TEXT("Radio1"), IE_Pressed, this, &AShooterPlayerController::Radio1);
	InputComponent->BindAction(TEXT("Radio2"), IE_Pressed, this, &AShooterPlayerController::Radio2);
	InputComponent->BindAction(TEXT("Radio3"), IE_Pressed, this, &AShooterPlayerController::Radio3);

	// The menus' keys take precedence over the pawn's only while the buy or the radio menu is open (UpdateMenuInput
	// pushes them): the number keys select weapons and Cross jumps otherwise.
	if (MenuInputComponent == nullptr)
	{
		MenuInputComponent = NewObject<UInputComponent>(this, TEXT("MenuInput"));
		UInputComponent& Menu = *MenuInputComponent;
		Menu.BindAction(TEXT("Menu"), IE_Pressed, this, &AShooterPlayerController::OnMenuPressed);
		Menu.BindAction(TEXT("MenuItem1"), IE_Pressed, this, &AShooterPlayerController::OnMenuItem1);
		Menu.BindAction(TEXT("MenuItem2"), IE_Pressed, this, &AShooterPlayerController::OnMenuItem2);
		Menu.BindAction(TEXT("MenuItem3"), IE_Pressed, this, &AShooterPlayerController::OnMenuItem3);
		Menu.BindAction(TEXT("MenuItem4"), IE_Pressed, this, &AShooterPlayerController::OnMenuItem4);
		Menu.BindAction(TEXT("MenuItem5"), IE_Pressed, this, &AShooterPlayerController::OnMenuItem5);
		Menu.BindAction(TEXT("MenuItem6"), IE_Pressed, this, &AShooterPlayerController::OnMenuItem6);
		Menu.BindAction(TEXT("MenuItem7"), IE_Pressed, this, &AShooterPlayerController::OnMenuItem7);
		Menu.BindAction(TEXT("MenuItem8"), IE_Pressed, this, &AShooterPlayerController::OnMenuItem8);
		Menu.BindAction(TEXT("MenuItem9"), IE_Pressed, this, &AShooterPlayerController::OnMenuItem9);
		Menu.BindAction(TEXT("MenuUp"), IE_Pressed, this, &AShooterPlayerController::OnBuyMenuUp);
		Menu.BindAction(TEXT("MenuDown"), IE_Pressed, this, &AShooterPlayerController::OnBuyMenuDown);
		Menu.BindAction(TEXT("MenuSelect"), IE_Pressed, this, &AShooterPlayerController::OnBuyMenuSelect);
	}
	// The spectator's keys (CS 1.6: attack for the next player, attack2 for the previous, jump for the mode), on the
	// stack only while spectating: the pawn's Fire and Jump are the shooter's otherwise.
	if (SpectatorInputComponent == nullptr)
	{
		SpectatorInputComponent = NewObject<UInputComponent>(this, TEXT("SpectatorInput"));
		SpectatorInputComponent->BindAction(TEXT("Fire"), IE_Pressed, this, &AShooterPlayerController::OnSpectateNext);
		SpectatorInputComponent->BindAction(
			TEXT("Targeting"), IE_Pressed, this, &AShooterPlayerController::OnSpectatePrev);
		SpectatorInputComponent->BindAction(
			TEXT("Jump"), IE_Pressed, this, &AShooterPlayerController::OnSpectateToggleFreeLook);
	}
	// A player spectating from its login on (a bot match's) began before its input existed.
	SetSpectatorInput(IsInState(NAME_Spectating));
}

int32 AShooterPlayerController::GetNumBuyMenuCategories()
{
	return static_cast<int32>(UE_ARRAY_COUNT(BuyMenuCategories));
}

const TCHAR* AShooterPlayerController::GetBuyMenuCategoryLabel(int32 Category)
{
	return FMath::IsWithin(Category, 0, GetNumBuyMenuCategories()) ? BuyMenuCategories[Category].Label : TEXT("Buy");
}

void AShooterPlayerController::GetBuyMenuEntries(
	TArray<FShooterBuyMenuEntry, TInlineAllocator<MaxBuyMenuEntries>>& OutEntries) const
{
	OutEntries.Reset();
	if (!FMath::IsWithin(BuyMenuCategory, 0, GetNumBuyMenuCategories()))
	{
		for (int32 Index = 0; Index < GetNumBuyMenuCategories(); ++Index)
		{
			const FBuyMenuCategory& Category = BuyMenuCategories[Index];
			OutEntries.Add({Category.Label, Category.DirectItem, Category.DirectItem != nullptr ? INDEX_NONE : Index});
		}
		return;
	}
	const AShooterPlayerState* State = GetPlayerState<AShooterPlayerState>();
	const EShooterTeam Team = State != nullptr ? State->GetTeam() : EShooterTeam::None;
	for (const TCHAR* Item : BuyMenuCategories[BuyMenuCategory].Items)
	{
		if (IsListedFor(Item, Team) && OutEntries.Num() < MaxBuyMenuEntries)
		{
			OutEntries.Add({Item, Item, INDEX_NONE});
		}
	}
}

void AShooterPlayerController::UpdateMenuInput()
{
	const bool bWanted = bBuyMenuOpen || RadioMenu != 0;
	if (MenuInputComponent == nullptr || bWanted == bMenuInputPushed)
	{
		return;
	}
	if (bWanted)
	{
		PushInputComponent(MenuInputComponent);
	}
	else
	{
		(void)PopInputComponent(MenuInputComponent);
	}
	bMenuInputPushed = bWanted;
}

void AShooterPlayerController::SetBuyMenuOpen(bool bOpen)
{
	// One menu at a time: the buy menu closes the radio's.
	if (bOpen)
	{
		RadioMenu = 0;
	}
	bBuyMenuOpen = bOpen;
	BuyMenuCategory = INDEX_NONE;
	BuyMenuSelection = 0;
	if (bOpen)
	{
		LastBuyMessage.Empty();
		BuyRefusalTime = -1.0f;
	}
	UpdateMenuInput();
}

void AShooterPlayerController::SetRadioMenu(int32 Menu)
{
	const int32 NewMenu = FMath::IsWithinInclusive(Menu, 1, NumRadioMenus) ? Menu : 0;
	if (NewMenu != 0 && bBuyMenuOpen)
	{
		SetBuyMenuOpen(false);
	}
	RadioMenu = NewMenu;
	UpdateMenuInput();
}

void AShooterPlayerController::ToggleRadioMenu(int32 Menu)
{
	if (RadioMenu == Menu)
	{
		SetRadioMenu(0);
		return;
	}
	// A dead or spectating player has no radio (CS).
	const AShooterCharacter* ShooterPawn = Cast<AShooterCharacter>(GetPawn());
	if (IsInState(NAME_Spectating) || ShooterPawn == nullptr || !ShooterPawn->IsAlive())
	{
		return;
	}
	SetRadioMenu(Menu);
}

void AShooterPlayerController::Radio1()
{
	ToggleRadioMenu(1);
}

void AShooterPlayerController::Radio2()
{
	ToggleRadioMenu(2);
}

void AShooterPlayerController::Radio3()
{
	ToggleRadioMenu(3);
}

bool AShooterPlayerController::SendRadio(EShooterRadioMessage Message)
{
	UWorld* World = GetWorld();
	AShooterGameMode* GameMode = World != nullptr ? World->GetAuthGameMode<AShooterGameMode>() : nullptr;
	return GameMode != nullptr && GameMode->SendRadioMessage(this, Message);
}

void AShooterPlayerController::OnRadioMenuItem(int32 Number)
{
	const TArrayView<const EShooterRadioMessage> Messages = GetRadioMenuMessages(RadioMenu);
	if (!Messages.IsValidIndex(Number - 1))
	{
		return;
	}
	(void)SendRadio(Messages[Number - 1]);
	SetRadioMenu(0);
}

void AShooterPlayerController::OnMenuItem(int32 Number)
{
	if (RadioMenu != 0)
	{
		OnRadioMenuItem(Number);
	}
	else
	{
		OnBuyMenuItem(Number);
	}
}

bool AShooterPlayerController::CanOpenBuyMenu(FString* OutReason) const
{
	auto Refuse = [OutReason](const TCHAR* Reason)
	{
		if (OutReason != nullptr)
		{
			*OutReason = Reason;
		}
		return false;
	};
	// A spectator (dead until the next round) has nothing to buy for, and the menu would take its Cross.
	if (IsInState(NAME_Spectating))
	{
		return Refuse(TEXT("spectating"));
	}
	const AShooterCharacter* ShooterPawn = Cast<AShooterCharacter>(GetPawn());
	if (ShooterPawn == nullptr || !ShooterPawn->IsAlive())
	{
		return Refuse(TEXT("dead"));
	}
	const UWorld* World = GetWorld();
	const AShooterGameMode* GameMode = World != nullptr ? World->GetAuthGameMode<AShooterGameMode>() : nullptr;
	return GameMode == nullptr || GameMode->CanBuy(*ShooterPawn, OutReason);
}

void AShooterPlayerController::OnBuyMenuRefused(const FString& Reason)
{
	const UWorld* World = GetWorld();
	LastBuyMessage = FString::Printf(TEXT("You cannot buy now: %s"), *Reason);
	BuyRefusalTime = World != nullptr ? World->GetTimeSeconds() : 0.0f;
	UE_LOG(LogShooter, Log, TEXT("Buy menu: %s"), *LastBuyMessage);
}

void AShooterPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	// Its keys would keep Cross (jump) and the number keys from the new pawn.
	SetBuyMenuOpen(false);
	SetRadioMenu(0);
	// ViewFrom's camera stays the view (the possession viewed the pawn) until ViewPawn.
	if (DebugCamera != nullptr && !DebugCamera->IsPendingKillPending() && PlayerCameraManager != nullptr)
	{
		PlayerCameraManager->SetViewTarget(DebugCamera);
	}
}

void AShooterPlayerController::BuyMenu()
{
	if (bBuyMenuOpen)
	{
		SetBuyMenuOpen(false);
		return;
	}
	FString Reason;
	if (!CanOpenBuyMenu(&Reason))
	{
		OnBuyMenuRefused(Reason);
		return;
	}
	SetBuyMenuOpen(true);
}

void AShooterPlayerController::OnBuyMenuPressed()
{
	BuyMenu();
}

void AShooterPlayerController::OnBuyMenuItem(int32 Number)
{
	TArray<FShooterBuyMenuEntry, TInlineAllocator<MaxBuyMenuEntries>> Entries;
	GetBuyMenuEntries(Entries);
	if (!bBuyMenuOpen || !Entries.IsValidIndex(Number - 1))
	{
		return;
	}
	const FShooterBuyMenuEntry& Entry = Entries[Number - 1];
	if (Entry.Item == nullptr)
	{
		// A category: its page.
		BuyMenuCategory = Entry.Category;
		BuyMenuSelection = 0;
		return;
	}
	Buy(Entry.Item);
	// CS: a purchase leaves the category (here the menu stays open on its first page).
	BuyMenuCategory = INDEX_NONE;
	BuyMenuSelection = 0;
}

void AShooterPlayerController::OnBuyMenuUp()
{
	if (!bBuyMenuOpen)
	{
		return;
	}
	TArray<FShooterBuyMenuEntry, TInlineAllocator<MaxBuyMenuEntries>> Entries;
	GetBuyMenuEntries(Entries);
	const int32 NumEntries = FMath::Max(1, Entries.Num());
	BuyMenuSelection = (BuyMenuSelection + NumEntries - 1) % NumEntries;
}

void AShooterPlayerController::OnBuyMenuDown()
{
	if (!bBuyMenuOpen)
	{
		return;
	}
	TArray<FShooterBuyMenuEntry, TInlineAllocator<MaxBuyMenuEntries>> Entries;
	GetBuyMenuEntries(Entries);
	BuyMenuSelection = (BuyMenuSelection + 1) % FMath::Max(1, Entries.Num());
}

void AShooterPlayerController::OnBuyMenuSelect()
{
	OnBuyMenuItem(BuyMenuSelection + 1);
}

void AShooterPlayerController::OnMenuItem1()
{
	OnMenuItem(1);
}

void AShooterPlayerController::OnMenuItem2()
{
	OnMenuItem(2);
}

void AShooterPlayerController::OnMenuItem3()
{
	OnMenuItem(3);
}

void AShooterPlayerController::OnMenuItem4()
{
	OnMenuItem(4);
}

void AShooterPlayerController::OnMenuItem5()
{
	OnMenuItem(5);
}

void AShooterPlayerController::OnMenuItem6()
{
	OnMenuItem(6);
}

void AShooterPlayerController::OnMenuItem7()
{
	OnMenuItem(7);
}

void AShooterPlayerController::OnMenuItem8()
{
	OnMenuItem(8);
}

void AShooterPlayerController::OnMenuItem9()
{
	OnMenuItem(9);
}

void AShooterPlayerController::Buy(FString Item)
{
	UWorld* World = GetWorld();
	AShooterGameMode* GameMode = World != nullptr ? World->GetAuthGameMode<AShooterGameMode>() : nullptr;
	AShooterCharacter* ShooterPawn = Cast<AShooterCharacter>(GetPawn());
	if (GameMode == nullptr || ShooterPawn == nullptr)
	{
		LastBuyMessage = TEXT("You cannot buy now");
		return;
	}
	FString Reason;
	LastBuyMessage = GameMode->Buy(ShooterPawn, Item, &Reason) ? FString::Printf(TEXT("Bought %s"), *Item)
															   : FString::Printf(TEXT("%s: %s"), *Item, *Reason);
	UE_LOG(LogShooter, Log, TEXT("Buy %s: %s"), *Item, *LastBuyMessage);
}

void AShooterPlayerController::BuyAmmo1()
{
	Buy(TEXT("primammo"));
}

void AShooterPlayerController::BuyAmmo2()
{
	Buy(TEXT("secammo"));
}

void AShooterPlayerController::Give(FString WeaponName)
{
	AShooterCharacter* ShooterPawn = Cast<AShooterCharacter>(GetPawn());
	UClass* WeaponClass = AShooterWeapon::FindWeaponClass(WeaponName);
	if (ShooterPawn == nullptr || WeaponClass == nullptr)
	{
		UE_LOG(LogShooter, Warning, TEXT("give: no weapon '%s'"), *WeaponName);
		return;
	}
	AShooterWeapon* Weapon = ShooterPawn->GiveWeapon(WeaponClass);
	if (Weapon != nullptr)
	{
		Weapon->RefillAmmo();
	}
	ShooterPawn->EquipWeapon(Weapon);
}

void AShooterPlayerController::God()
{
	if (AShooterCharacter* ShooterPawn = Cast<AShooterCharacter>(GetPawn()))
	{
		ShooterPawn->SetGodMode(!ShooterPawn->IsGodMode());
		UE_LOG(LogShooter, Log, TEXT("god mode %s"), ShooterPawn->IsGodMode() ? TEXT("ON") : TEXT("OFF"));
	}
}

void AShooterPlayerController::Kill()
{
	if (AShooterCharacter* ShooterPawn = Cast<AShooterCharacter>(GetPawn()))
	{
		ShooterPawn->Suicide();
	}
}

void AShooterPlayerController::OnScoreboardPressed()
{
	bShowScoreboard = true;
}

void AShooterPlayerController::OnScoreboardReleased()
{
	bShowScoreboard = false;
}

void AShooterPlayerController::OnMenuPressed()
{
	// Escape closes the radio menu; it goes back to the buy menu's first page, and closes it there (ShooterGame has no
	// pause menu).
	if (RadioMenu != 0)
	{
		SetRadioMenu(0);
	}
	else if (bBuyMenuOpen && BuyMenuCategory != INDEX_NONE)
	{
		BuyMenuCategory = INDEX_NONE;
		BuyMenuSelection = 0;
	}
	else if (bBuyMenuOpen)
	{
		SetBuyMenuOpen(false);
	}
}

void AShooterPlayerController::NotifyHitConfirmed(bool bHeadshot, bool bKilled)
{
	const UWorld* World = GetWorld();
	LastHitTime = World != nullptr ? World->GetTimeSeconds() : 0.0f;
	bLastHitHeadshot = bHeadshot;
	bLastHitKill = bKilled;
}

void AShooterPlayerController::ViewFrom(float X, float Y, float Z, float Pitch, float Yaw)
{
	UWorld* World = GetWorld();
	if (World == nullptr || PlayerCameraManager == nullptr)
	{
		return;
	}
	if (DebugCamera == nullptr || DebugCamera->IsPendingKillPending())
	{
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.ObjectFlags |= RF_Transient;
		DebugCamera = World->SpawnActor<ACameraActor>(FVector(X, Y, Z), FRotator::ZeroRotator, SpawnInfo);
	}
	if (DebugCamera == nullptr)
	{
		return;
	}
	UCameraComponent* Camera = DebugCamera->GetCameraComponent();
	Camera->SetMode(ECameraMode::FreeLook);
	Camera->SetEyeLocation(FVector(X, Y, Z));
	Camera->SetViewRotation(FRotator(Pitch, Yaw, 0.0f));
	PlayerCameraManager->SetViewTarget(DebugCamera);
	UE_LOG(LogShooter, Log, TEXT("ViewFrom (%.0f, %.0f, %.0f) pitch %.0f yaw %.0f"), static_cast<double>(X),
		static_cast<double>(Y), static_cast<double>(Z), static_cast<double>(Pitch), static_cast<double>(Yaw));
}

void AShooterPlayerController::ThrowGrenade()
{
	AShooterCharacter* ShooterPawn = Cast<AShooterCharacter>(GetPawn());
	if (ShooterPawn == nullptr)
	{
		return;
	}
	ShooterPawn->SelectSlot(EShooterWeaponSlot::Grenade);
	const AShooterWeapon* Grenade = ShooterPawn->GetWeapon();
	// Thrown when drawn (the draw's time, with a margin).
	constexpr float DrawMargin = 0.2f;
	GetWorldTimerManager().SetTimer(TimerHandle_ThrowGrenade, this, &AShooterPlayerController::OnThrowGrenadeTimer,
		(Grenade != nullptr ? Grenade->EquipDuration : 0.5f) + 1.0f + DrawMargin);
}

void AShooterPlayerController::OnThrowGrenadeTimer()
{
	AShooterCharacter* ShooterPawn = Cast<AShooterCharacter>(GetPawn());
	if (ShooterPawn == nullptr)
	{
		return;
	}
	// The freeze holds the throw: try again until the round is live.
	const AShooterWeapon* Weapon = ShooterPawn->GetWeapon();
	if (Cast<AShooterWeapon_Projectile>(Weapon) == nullptr)
	{
		return;
	}
	if (ShooterPawn->IsFrozen() || !Weapon->CanFire())
	{
		constexpr float RetryDelay = 0.5f;
		GetWorldTimerManager().SetTimer(
			TimerHandle_ThrowGrenade, this, &AShooterPlayerController::OnThrowGrenadeTimer, RetryDelay);
		return;
	}
	ShooterPawn->StartWeaponFire();
	ShooterPawn->StopWeaponFire();
}

void AShooterPlayerController::ViewPawn()
{
	if (DebugCamera != nullptr)
	{
		(void)DebugCamera->Destroy();
		DebugCamera = nullptr;
	}
	if (PlayerCameraManager != nullptr)
	{
		PlayerCameraManager->SetViewTarget(nullptr);
	}
}
