#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/PlayerController.h"
#include "ShooterTypes.h"
#include "ShooterPlayerController.generated.h"

class ACameraActor;
class AShooterCharacter;
class UForceFeedbackEffect;
class UInputComponent;
class UShooterPersistentUser;
class USoundWave;
struct FShooterRadioEntry;

/** A line of the buy menu's page: a category that opens (Category), or an item that buys (Item). */
struct FShooterBuyMenuEntry
{
	/** What the line says: the category's name, or the item's buy name. */
	const TCHAR* Label = nullptr;
	/** The buy name (AShooterGameMode::Buy), null for a category. */
	const TCHAR* Item = nullptr;
	/** The category it opens (AShooterPlayerController::GetBuyMenuCategory), INDEX_NONE for an item. */
	int32 Category = INDEX_NONE;
};

/**
 * The human player's controller (UE ShooterGame: AShooterPlayerController): the scoreboard (Tab, held; Select), the
 * buy menu (B; Start), the hit marker's state, CS's console commands and a debug view for captures.
 *
 * The buy menu is CS's: its first page lists the categories (1 Pistols, 2 SMGs, 3 Rifles, 4 Primary ammo, 5 Secondary
 * ammo, 6 Equipment); a category's page lists its items the player's team may buy (the rifles: the AK-47 for the
 * terrorists, the M4A1 for the counter-terrorists, and the AWP), and buying one goes back to the first page. The ammo
 * lines buy a box of the primary's or the pistol's ammunition, as `,` and `.` do at any time (CS: buyammo1 /
 * buyammo2).
 *
 * While the buy menu is open its own input component is on top of the input stack (UE: PushInputComponent), so its
 * keys are not the pawn's then: the lines on the number keys (MenuItem1..7), the D-pad's up and down moving the
 * highlighted line (MenuUp / MenuDown) and Cross choosing it (MenuSelect), Esc or Circle going back to the first page
 * or, there, closing it (Menu). It opens only when its player could buy (CanOpenBuyMenu: alive and playing, in the
 * team's buy zone, within the buy time), and it closes by itself when that stops (CS: the buy time's end closes it); a
 * refused or closed menu leaves its reason in the last buy's message, which the HUD shows for a moment.
 *
 * The radio (CS 1.6's; ps2-shipping N30e): Z, X and C (Radio1, Radio2, Radio3) open its three menus; the menus'
 * input component takes the number keys (1 to 9: the menu's messages, AShooterGameMode::SendRadioMessage to the team)
 * and Esc while one is open, as it does for the buy menu, and opening one closes the other. A message to the player's
 * team plays its radio sound (HearRadio, ps2-shipping N30f): a squelch and a tone pattern a menu (the commands, the
 * group commands, the responses and reports), and a cue each for "Fire in the hole!" and "Bomb has been planted." (CS
 * speaks them; the tones are the game's own).
 *
 * Console (Exec): `Buy <item>` (glock, usp, deagle, mp5, ak47, m4a1, awp, hegrenade, primammo, secammo, vest,
 * vesthelm, defuser: AShooterGameMode::Buy), `BuyAmmo1` / `BuyAmmo2` (CS's), `buymenu` (opens or closes the menu, as
 * B), `radio1` / `radio2` / `radio3` (the radio menus, as Z, X and C),`ViewNextPlayer` / `ViewPrevPlayer` (CS:
 * spec_next / spec_prev) and the cheats `give <weapon>` (a weapon by name, free, anywhere, its reserve full), `god` (no
 * damage, toggles) and `kill` (suicide).
 *
 * Death and spectating (CS 1.6): when its pawn dies (StartDeathCam) the player spectates from the corpse's eyes (the
 * spectator pawn, held there) looking at the killer for DeathCamDuration (the death cam). Then it watches its living
 * teammates through their eyes, and when none is left it flies free (the free look). While spectating the spectator's
 * own input component is on top of the input stack: Fire (or `ViewNextPlayer`) watches the next teammate, Targeting
 * (`ViewPrevPlayer`) the one before, Jump switches between the teammates and the free look; Fire during the death cam
 * ends it. When the watched teammate dies the next one is watched. The HUD names who is watched.
 *
 * Damage (NotifyTakeDamage): the pawn remembers where its last damage came from, and when, for the HUD's damage
 * direction indicator.
 *
 * `-BotMatchSpectate`: while spectating, the player always watches a living player through their eyes (the next one
 * when the one watched dies): a bot match seen as a player sees it, for MeasurePS2's measured runs.
 */
UCLASS(Config = Game)
class SHOOTERGAME_API AShooterPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AShooterPlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * Binds Scoreboard and BuyMenu on the controller's input component, and the buy menu's actions on its own
	 * (UE: SetupInputComponent).
	 */
	void SetupInputComponent() override;

	/** The most lines a page of the buy menu has. */
	static constexpr int32 MaxBuyMenuEntries = 7;
	/** The number of the buy menu's categories (the first page's lines). */
	[[nodiscard]] static int32 GetNumBuyMenuCategories();
	/** A category's name ("Rifles"), "Buy" for the first page (INDEX_NONE). */
	[[nodiscard]] static const TCHAR* GetBuyMenuCategoryLabel(int32 Category);
	/**
	 * The lines of the buy menu's page now, in the order of their number keys: the categories on the first page, else
	 * the category's items the player's team may buy (nothing allocated: the names are static).
	 */
	void GetBuyMenuEntries(TArray<FShooterBuyMenuEntry, TInlineAllocator<MaxBuyMenuEntries>>& OutEntries) const;
	/** The category whose page is open, INDEX_NONE for the first page. */
	[[nodiscard]] int32 GetBuyMenuCategory() const
	{
		return BuyMenuCategory;
	}

	/** The buy menu is open (B): the HUD draws it and the number keys buy. */
	[[nodiscard]] bool IsBuyMenuOpen() const
	{
		return bBuyMenuOpen;
	}
	/** Opens or closes the menu as it is told (its keys on the input stack); BuyMenu checks the rules first. */
	void SetBuyMenuOpen(bool bOpen);
	/**
	 * The buy menu may be open: the player plays a live pawn (not dead, not spectating) that may buy now
	 * (AShooterGameMode::CanBuy: its buy zone, the buy time; no game mode sets no rule). OutReason says why not.
	 */
	[[nodiscard]] bool CanOpenBuyMenu(FString* OutReason = nullptr) const;
	/** The world time the menu was last refused or closed by the rules (negative: not since it last opened). */
	[[nodiscard]] float GetBuyRefusalTime() const
	{
		return BuyRefusalTime;
	}
	/** The line the D-pad highlights in the buy menu (an index of GetBuyMenuEntries). */
	[[nodiscard]] int32 GetBuyMenuSelection() const
	{
		return BuyMenuSelection;
	}
	/** The last buy's result, for the HUD ("bought ak47", "not enough money"). */
	[[nodiscard]] const FString& GetLastBuyMessage() const
	{
		return LastBuyMessage;
	}

	/** Buys an item for the pawn (AShooterGameMode::Buy). */
	UFUNCTION(Exec)
	void Buy(FString Item);

	/** A box of the primary's ammunition (CS: buyammo1, the `,` key). */
	UFUNCTION(Exec)
	void BuyAmmo1();

	/** A box of the pistol's ammunition (CS: buyammo2, the `.` key). */
	UFUNCTION(Exec)
	void BuyAmmo2();

	/** Opens or closes the buy menu (CS: buymenu); it does not open when CanOpenBuyMenu refuses. */
	UFUNCTION(Exec)
	void BuyMenu();

	/** Cheat: a weapon by name, free (CS: give weapon_ak47). */
	UFUNCTION(Exec)
	void Give(FString WeaponName);

	/** Cheat: the pawn takes no damage; again to take it. */
	UFUNCTION(Exec)
	void God();

	/** The pawn dies (CS: kill). */
	UFUNCTION(Exec)
	void Kill();

	// The radio (CS 1.6's; ps2-shipping N30e)

	/**
	 * Opens radio menu Menu (1 to 3: CS's radio1, radio2 and radio3, on Z, X and C), closing the buy menu; the same
	 * one again closes it. It opens only for a living player (not spectating). While it is open the number keys send
	 * its messages (GetRadioMenuMessages) and close it, and Esc closes it.
	 */
	void ToggleRadioMenu(int32 Menu);
	/** The radio menu open (1 to 3), 0 when none. */
	[[nodiscard]] int32 GetRadioMenu() const
	{
		return RadioMenu;
	}
	/** Opens or closes a radio menu (0: closed) as it is told. */
	void SetRadioMenu(int32 Menu);

	/** CS's radio1 (Z): "Cover me!", "You take the point.", ... */
	UFUNCTION(Exec)
	void Radio1();
	/** CS's radio2 (X): "Go go go!", "Team, fall back!", ... */
	UFUNCTION(Exec)
	void Radio2();
	/** CS's radio3 (C): "Affirmative.", "Enemy spotted.", "Need backup.", ... */
	UFUNCTION(Exec)
	void Radio3();

	/** Radios Message to the player's team (AShooterGameMode::SendRadioMessage); true when sent. */
	bool SendRadio(EShooterRadioMessage Message);

	/** The radio's sounds: a menu's messages, "Fire in the hole!" and "Bomb has been planted." (the class comment). */
	UPROPERTY(Config)
	FSoftObjectPath RadioCommandSoundName;

	UPROPERTY(Config)
	FSoftObjectPath RadioGroupSoundName;

	UPROPERTY(Config)
	FSoftObjectPath RadioReportSoundName;

	UPROPERTY(Config)
	FSoftObjectPath RadioFireInTheHoleSoundName;

	UPROPERTY(Config)
	FSoftObjectPath RadioBombPlantedSoundName;

	/** The sound a radio message plays (null without one). */
	[[nodiscard]] USoundWave* GetRadioSound(EShooterRadioMessage Message) const;
	/** A local player hears its team's message (AShooterGameMode::SendRadioMessage): its sound, 2D. */
	void HearRadio(const FShooterRadioEntry& Entry);
	/** The sound of the last message heard (the tests). */
	[[nodiscard]] USoundWave* GetLastRadioSound() const
	{
		return LastRadioSound;
	}

	/** Tab is held: the HUD lists the players by team (AShooterHUD). */
	[[nodiscard]] bool IsScoreboardShown() const
	{
		return bShowScoreboard;
	}

	/**
	 * Debug: views the map from a point, looking along Pitch / Yaw, through a camera actor (the view target) until
	 * ViewPawn, also after the player's pawn spawns at the round's start: `-ExecCmds="ViewFrom 0 0 4000 -89 0"
	 * -Screenshot=Top.bmp` for a top-down capture.
	 */
	UFUNCTION(Exec)
	void ViewFrom(float X, float Y, float Z, float Pitch, float Yaw);

	/**
	 * A shot of this player hurt a character (UE ShooterGame: the HUD's hit notify; CS's hit sound): the HUD draws the
	 * hit marker for HitMarkerDuration, red for a kill.
	 */
	void NotifyHitConfirmed(bool bHeadshot, bool bKilled);
	/** The world time of the last confirmed hit (negative before the first), and what it was. */
	[[nodiscard]] float GetLastHitTime() const
	{
		return LastHitTime;
	}
	[[nodiscard]] bool WasLastHitHeadshot() const
	{
		return bLastHitHeadshot;
	}
	[[nodiscard]] bool WasLastHitKill() const
	{
		return bLastHitKill;
	}

	/** Debug: back to the pawn's view. */
	UFUNCTION(Exec)
	void ViewPawn();

	/**
	 * Debug: draws the grenade slot and throws the grenade once it is out (a capture's smoke or flash: `-ExecCmds="give
	 * smokegrenade;ThrowGrenade"`).
	 */
	UFUNCTION(Exec)
	void ThrowGrenade();

	/**
	 * While spectating, views the next living player through their eyes (CS: spec_next): a player on a team watches
	 * only its team, one without a team everyone. The order is the game state's player list, the same every run. It
	 * ends the death cam; with nobody to watch the player looks around freely.
	 */
	UFUNCTION(Exec)
	void ViewNextPlayer();
	/** The same the other way (CS: spec_prev). */
	UFUNCTION(Exec)
	void ViewPrevPlayer();
	/** The living player viewed while spectating, or null. */
	[[nodiscard]] AShooterCharacter* GetViewedPlayer() const;

	/** Seconds the death cam looks at the killer before the player watches its teammates (CS: about 2). */
	UPROPERTY(Config)
	float DeathCamDuration = 2.0f;

	/**
	 * The pawn died (AShooterCharacter's death): the player spectates, first from the corpse's eyes looking at Killer
	 * (null: a suicide or the world; the view holds) for DeathCamDuration, then its teammates.
	 */
	void StartDeathCam(AActor* Killer);
	/** What the spectating player sees now (None while playing). */
	[[nodiscard]] EShooterSpectatorMode GetSpectatorMode() const
	{
		return IsInState(NAME_Spectating) ? SpectatorMode : EShooterSpectatorMode::None;
	}
	/** The actor the death cam looks at (null without one). */
	[[nodiscard]] AActor* GetDeathCamTarget() const
	{
		return DeathCamTarget.Get();
	}
	/** The killer's name at the death (empty: a suicide or the world), for the HUD. */
	[[nodiscard]] const FString& GetDeathCamKillerName() const
	{
		return DeathCamKillerName;
	}

	/** The pawn was hurt by damage from SourceLocation (the shooter, the grenade, the bomb): the HUD's indicator. */
	void NotifyTakeDamage(const FVector& SourceLocation);
	/** The world time of the last damage taken (negative before any), and where it came from. */
	[[nodiscard]] float GetLastDamageTime() const
	{
		return LastDamageTime;
	}
	[[nodiscard]] const FVector& GetLastDamageSourceLocation() const
	{
		return LastDamageSourceLocation;
	}

	void PlayerTick(float DeltaTime) override;
	/** Closes the buy menu when its player can no longer buy (the buy time's end, the buy zone left). */
	void Tick(float DeltaSeconds) override;

	// The player's options (Docs/PLANS/ps2-shipping.md N24, UShooterPersistentUser): each command applies its option
	// and saves them (the memory card on the PS2).

	/** The aim's scale (1: the game's): the mouse's degrees a pixel and the right stick's rates. */
	UFUNCTION(Exec)
	void SetSensitivity(float Scale);
	/** 1 inverts the Y axis (up looks down) of the mouse and the right stick, 0 does not. */
	UFUNCTION(Exec)
	void SetInvertY(int32 Invert);
	/** The sound's volume, 0 to 1. */
	UFUNCTION(Exec)
	void SetVolume(float Volume);
	/** The crosshair's colour, each channel 0 to 1. */
	UFUNCTION(Exec)
	void SetCrosshairColor(float Red, float Green, float Blue);

	/** The options (loaded at BeginPlay for a player at a screen; the defaults otherwise), never null once playing. */
	[[nodiscard]] UShooterPersistentUser* GetPersistentUser();
	/** Uses the options: the input's sensitivity and Y axis, the audio's volume, the HUD's crosshair. */
	void ApplyPersistentUser();
	/** The aim's scale the pawn's stick rates take (1 without options). */
	[[nodiscard]] float GetAimSensitivity() const;

	// Force feedback (N24; UE ShooterGame's): the DualShock's small motor on each shot, the large one when hurt and
	// near an explosion.

	/** A shot of this player's weapon. */
	void PlayFireForceFeedback();
	/** This player's pawn was hurt. */
	void PlayHitForceFeedback();
	/** An explosion at Origin shakes the local players whose view is within twice Radius. */
	static void PlayExplosionForceFeedback(UWorld* World, const FVector& Origin, float Radius);

	UPROPERTY()
	UForceFeedbackEffect* FireForceFeedback = nullptr;
	UPROPERTY()
	UForceFeedbackEffect* HitForceFeedback = nullptr;
	UPROPERTY()
	UForceFeedbackEffect* ExplosionForceFeedback = nullptr;

protected:
	void BeginPlay() override;
	/** A new pawn (the spectator at death, a shooter at a round's start) closes the buy menu (CS). */
	void OnPossess(APawn* InPawn) override;
	/** Spectating begins in the free look, the spectator's keys on top of the input stack. */
	void BeginSpectatingState() override;
	/** The death cam ends and the spectator's keys go. */
	void EndSpectatingState() override;

private:
	/** Watches the living player Step places from the one watched (1 next, -1 previous); the free look without any. */
	void ViewPlayer(int32 Step);
	/** The death cam's time is up: the teammates (or the free look). */
	void OnDeathCamTimer();
	/** The death cam looks at its target from where it holds. */
	void UpdateDeathCam();
	/** Flies free from where the camera is (UE: the spectator pawn as the view target). */
	void BeginFreeLook();
	/** The spectator's keys (on the input stack while spectating). */
	void OnSpectateNext();
	void OnSpectatePrev();
	void OnSpectateToggleFreeLook();
	/** Puts the spectator's keys on the input stack, or takes them off. */
	void SetSpectatorInput(bool bEnabled);

	void OnScoreboardPressed();
	void OnScoreboardReleased();
	void OnBuyMenuPressed();
	/** A number key while a menu is open (1-based): the radio menu's message, else the buy menu's line. */
	void OnMenuItem(int32 Number);
	void OnMenuItem1();
	void OnMenuItem2();
	void OnMenuItem3();
	void OnMenuItem4();
	void OnMenuItem5();
	void OnMenuItem6();
	void OnMenuItem7();
	void OnMenuItem8();
	void OnMenuItem9();
	/** A number key in the buy menu: opens that category or buys that item (1-based). */
	void OnBuyMenuItem(int32 Number);
	/** A number key in the radio menu: sends that message and closes the menu. */
	void OnRadioMenuItem(int32 Number);
	/** Esc: closes the radio menu; in the buy menu back to its first page, or closes it there. */
	void OnMenuPressed();
	/** The menus' keys go on the input stack while the buy or the radio menu is open, and off when neither is. */
	void UpdateMenuInput();
	/** The menu refused or closed by the rules: the reason becomes the last buy's message, for the HUD. */
	void OnBuyMenuRefused(const FString& Reason);
	/** The D-pad in the buy menu: the item above or below (wrapping), and Cross buys it. */
	void OnBuyMenuUp();
	void OnBuyMenuDown();
	void OnBuyMenuSelect();

	bool bShowScoreboard = false;
	bool bBuyMenuOpen = false;
	int32 BuyMenuSelection = 0;
	int32 BuyMenuCategory = INDEX_NONE;
	FString LastBuyMessage;
	float BuyRefusalTime = -1.0f;

	/** The radio menu open (1 to 3), 0 when none. */
	int32 RadioMenu = 0;

	/** The radio's sounds, loaded: the three menus', "Fire in the hole!" and "Bomb has been planted.". */
	UPROPERTY(Transient)
	TArray<USoundWave*> RadioSounds;

	UPROPERTY(Transient)
	USoundWave* LastRadioSound = nullptr;

	/** The menus' keys (the buy menu's and the radio menu's), on the input stack while one is open. */
	UPROPERTY(Transient)
	UInputComponent* MenuInputComponent = nullptr;
	bool bMenuInputPushed = false;

	float LastHitTime = -1.0f;
	bool bLastHitHeadshot = false;
	bool bLastHitKill = false;

	/** -BotMatchSpectate: while spectating, a living player is always viewed. */
	bool bFollowPlayers = false;

	/** The spectator's keys: Fire, Targeting, Jump (SetSpectatorInput). */
	UPROPERTY(Transient)
	UInputComponent* SpectatorInputComponent = nullptr;
	bool bSpectatorInputPushed = false;

	EShooterSpectatorMode SpectatorMode = EShooterSpectatorMode::None;
	/** The death cam's end (OnDeathCamTimer). */
	FTimerHandle TimerHandle_DeathCam;
	TWeakObjectPtr<AActor> DeathCamTarget;
	FString DeathCamKillerName;

	float LastDamageTime = -1.0f;
	FVector LastDamageSourceLocation = FVector::ZeroVector;

	/** ThrowGrenade's throw, once the grenade is out. */
	FTimerHandle TimerHandle_ThrowGrenade;
	void OnThrowGrenadeTimer();

	/** The camera of ViewFrom. */
	UPROPERTY(Transient)
	ACameraActor* DebugCamera = nullptr;

	/** The options (GetPersistentUser). */
	UPROPERTY(Transient)
	UShooterPersistentUser* PersistentUser = nullptr;
	/** The mouse's sensitivity before the options scale it (the input config's), once read. */
	float BaseMouseSensitivity = -1.0f;
	/** Saves the options, when they belong to a player at a screen. */
	void SavePersistentUser();
};
