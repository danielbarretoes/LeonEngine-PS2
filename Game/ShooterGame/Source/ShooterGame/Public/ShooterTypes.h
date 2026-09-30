#pragma once

#include "CoreMinimal.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/SoftObjectPath.h"
#include "ShooterTypes.generated.h"

class UAnimMontage;
class USoundWave;

/**
 * The two sides of a match (Counter-Strike's counter-terrorists and terrorists; UE ShooterGame numbers its teams).
 * None is a player not placed in a team yet.
 */
UENUM()
enum class EShooterTeam : uint8
{
	None,
	CT,
	T,
};

/**
 * The inventory slot a weapon takes (Counter-Strike's slots 1 to 4): a player carries one weapon per slot. The number
 * keys select them (DefaultInput.ini: PrimaryWeapon, SecondaryWeapon, Knife, Grenade).
 */
UENUM()
enum class EShooterWeaponSlot : uint8
{
	/** Rifles, sub-machine guns and sniper rifles (CS slot 1). */
	Primary,
	/** Pistols (CS slot 2). */
	Secondary,
	/** The knife (CS slot 3): every player's, never dropped. */
	Knife,
	/** Grenades (CS slot 4). */
	Grenade,
};

/** What a weapon is doing (UE ShooterGame: EWeaponState). */
UENUM()
enum class EShooterWeaponState : uint8
{
	Idle,
	Firing,
	Reloading,
	Equipping,
};

/**
 * Where a shot struck a character (Counter-Strike's hit groups, HITGROUP_*): AShooterCharacter::GetHitGroup tells them
 * apart by height bands and sides of the capsule. The victim scales the damage by the group (the head x4, the stomach
 * x1.25, the legs x0.75) and armor covers every group but the legs (the head only with a helmet).
 */
UENUM()
enum class EShooterHitGroup : uint8
{
	/** Damage that struck no point (a blast, the world): armor covers it. */
	Generic,
	/** The top HeadHeight cm of the capsule. */
	Head,
	Chest,
	Stomach,
	LeftArm,
	RightArm,
	LeftLeg,
	RightLeg,
};

/**
 * A sound for each of the game's surfaces (UE ShooterGame: AShooterImpactEffect's DefaultSound, ConcreteSound, ...;
 * ShooterGame.h's SHOOTER_SURFACE_*): each surface's variants, a surface without any takes Default's. The config
 * writes it as `(Default=("/Game/Sounds/S_Step_Concrete_L.S_Step_Concrete_L"),Dirt=(...),...)`.
 */
USTRUCT()
struct SHOOTERGAME_API FShooterSurfaceSounds
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FSoftObjectPath> Default;

	UPROPERTY()
	TArray<FSoftObjectPath> Concrete;

	UPROPERTY()
	TArray<FSoftObjectPath> Dirt;

	UPROPERTY()
	TArray<FSoftObjectPath> Metal;

	UPROPERTY()
	TArray<FSoftObjectPath> Wood;

	UPROPERTY()
	TArray<FSoftObjectPath> Tile;

	UPROPERTY()
	TArray<FSoftObjectPath> Glass;

	UPROPERTY()
	TArray<FSoftObjectPath> Computer;

	UPROPERTY()
	TArray<FSoftObjectPath> Flesh;

	/** The variants of a surface, as the config names them (empty for a surface the game does not name). */
	[[nodiscard]] const TArray<FSoftObjectPath>& GetPaths(EPhysicalSurface Surface) const;
};

/**
 * The loaded sounds of an FShooterSurfaceSounds (Load), held for the garbage collector: Get picks a surface's variant
 * (Default's when the surface has none).
 */
USTRUCT()
struct SHOOTERGAME_API FShooterSurfaceSoundSet
{
	GENERATED_BODY()

	/** The variants a surface keeps (Counter-Strike has up to four a surface). */
	static constexpr int32 MaxVariants = 4;
	/** The surfaces FShooterSurfaceSounds names: Default and SurfaceType1 to SurfaceType8. */
	static constexpr int32 NumSurfaces = 9;

	/** MaxVariants slots a surface, in surface order; null where the config names nothing or the sound is missing. */
	UPROPERTY(Transient)
	TArray<USoundWave*> Sounds;

	/** Loads the sounds Paths names (those whose package does not exist are left out). */
	void Load(const FShooterSurfaceSounds& Paths);
	/** Variant (wrapped to the surface's count) of Surface's sounds, else of Default's; null when neither has one. */
	[[nodiscard]] USoundWave* Get(EPhysicalSurface Surface, int32 Variant) const;
	/** How many sounds Surface has (none: it plays Default's). */
	[[nodiscard]] int32 GetNumVariants(EPhysicalSurface Surface) const;
};

/**
 * The asset a config path names, or null when the path is empty or its package does not exist (no art yet). An asset in
 * memory (the game mode's preload, ps2-shipping N24) is found by a lookup without asking the file system, so a spawn
 * resolves its assets cheaply (N24b: ten pawns and their weapons asked it about 600 times at the round's start).
 */
[[nodiscard]] SHOOTERGAME_API UObject* LoadShooterObject(const FSoftObjectPath& Path);

template <class T>
[[nodiscard]] T* LoadShooterAsset(const FSoftObjectPath& Path)
{
	return Cast<T>(LoadShooterObject(Path));
}

/** The sound of an asset path (LoadShooterAsset). */
[[nodiscard]] SHOOTERGAME_API USoundWave* LoadShooterSound(const FSoftObjectPath& Path);

/**
 * The phase of a round (Counter-Strike's round flow; AShooterGameMode drives it, AShooterGameState holds it). Warmup
 * lasts until both teams have a player; each round is Freeze (nobody moves, buying), Live (the round's time), then
 * RoundEnd (the result shows) before the next Freeze; MatchEnd after the last round.
 */
UENUM()
enum class EShooterRoundState : uint8
{
	Warmup,
	Freeze,
	Live,
	RoundEnd,
	MatchEnd,
};

/** Where the bomb is (AShooterBomb). */
UENUM()
enum class EShooterBombState : uint8
{
	/** Not in play (no round, or no terrorist to carry it). */
	None,
	/** A terrorist carries it. */
	Carried,
	/** On the floor, for a terrorist to pick up. */
	Dropped,
	/** Planted in a bomb site, ticking. */
	Planted,
	Defused,
	Exploded,
};

/** Why a round ended (Counter-Strike's round end messages). */
UENUM()
enum class EShooterRoundEndReason : uint8
{
	None,
	/** "Target Successfully Bombed!": the planted bomb exploded (T). */
	TargetBombed,
	/** "The bomb has been defused!" (CT). */
	BombDefused,
	/** "Terrorists Win!": every CT is dead (T). */
	CTsEliminated,
	/** "Counter-Terrorists Win!": every T is dead with no bomb planted (CT). */
	TerroristsEliminated,
	/** "Target has been saved!": the time ran out with no bomb planted (CT). */
	TargetSaved,
	/** "Round Draw!": both teams died at once (nobody scores). */
	Draw,
};

/** What a dead or watching player sees (AShooterPlayerController; CS 1.6's spectator modes). */
UENUM()
enum class EShooterSpectatorMode : uint8
{
	/** Playing: not spectating. */
	None,
	/** From the corpse's eyes, looking at the killer, for a moment after death. */
	DeathCam,
	/** Through a living player's eyes. */
	Player,
	/** Flying free. */
	FreeLook,
};

/**
 * An animation of the pawn in both views (UE ShooterGame: FWeaponAnim): the montage of the first-person arms and the
 * one of the body the others see (AShooterCharacter::PlayPawnMontages). A weapon's fire, reload and draw; the bomb's
 * plant and defuse. Either may be null (no art yet: Docs/PLANS/ps2-shipping.md N27).
 */
USTRUCT()
struct SHOOTERGAME_API FShooterWeaponAnim
{
	GENERATED_BODY()

	UPROPERTY()
	UAnimMontage* Pawn1P = nullptr;

	UPROPERTY()
	UAnimMontage* Pawn3P = nullptr;
};

/**
 * A state's share of a weapon's kick (AShooterWeapon_Instant::GetRecoilScale): CS 1.6's KickBack arguments in that
 * state's branch of the weapon's PrimaryAttack over the standing branch's. Up scales the kick up (RecoilPitch and its
 * random part), Lateral the sideways kick (RecoilYawRandom). 1 and 1 is the standing kick.
 */
USTRUCT()
struct SHOOTERGAME_API FShooterRecoilScale
{
	GENERATED_BODY()

	UPROPERTY()
	float Up = 1.0f;

	UPROPERTY()
	float Lateral = 1.0f;
};

/**
 * A team's purchases for a round (ps2-shipping N30e; AShooterGameMode decides it for each team when the round starts,
 * and the bots buy by it: AShooterAIController::BuyForRound). CS's economy: the first round of each half is the pistol
 * round, a team that can equip most of its players buys in full, one that cannot saves (eco) unless it has lost too
 * many rounds in a row, it won the last one or the half ends, when it spends what it has (a force-buy).
 */
enum class EShooterBuyPlan : uint8
{
	/** The first round of a half: kevlar with the $800 (nothing else fits). */
	Pistol,
	/** Save: only a player who can afford the full buy (the team's rifle and kevlar with a helmet) buys. */
	Eco,
	/** The best each player can afford: the rifle, else an SMG with kevlar, else a Desert Eagle with kevlar. */
	Force,
	/** Most of the team can afford the rifle and armor: everyone buys. */
	Full,
};

/** The plan's name for the log ("Pistol", "Eco", "Force", "Full"). */
[[nodiscard]] SHOOTERGAME_API const TCHAR* GetBuyPlanName(EShooterBuyPlan Plan);

/**
 * Counter-Strike 1.6's radio messages (ps2-shipping N30e; AShooterGameMode::SendRadioMessage): the three menus of its
 * radio keys (Z: radio1, X: radio2, C: radio3, in their order) and the two a player sends by itself (a grenade's
 * throw, the bomb planted by a bot). Only the sender's team hears them.
 */
enum class EShooterRadioMessage : uint8
{
	None,
	// radio1 (Z)
	CoverMe,
	YouTakeThePoint,
	HoldThisPosition,
	RegroupTeam,
	FollowMe,
	TakingFire,
	// radio2 (X)
	GoGoGo,
	TeamFallBack,
	StickTogether,
	GetInPosition,
	StormTheFront,
	ReportIn,
	// radio3 (C)
	Affirmative,
	EnemySpotted,
	NeedBackup,
	SectorClear,
	InPosition,
	ReportingIn,
	GetOut,
	Negative,
	EnemyDown,
	// Sent by themselves
	FireInTheHole,
	BombPlanted,
};

/** The most lines a radio menu has (radio3's nine). */
constexpr int32 MaxRadioMenuMessages = 9;

/** The radio menus (radio1, radio2, radio3). */
constexpr int32 NumRadioMenus = 3;

/** What a radio message says (CS 1.6's text: "Enemy spotted.", "Fire in the hole!"). */
[[nodiscard]] SHOOTERGAME_API const TCHAR* GetRadioMessageText(EShooterRadioMessage Message);

/** The messages of a radio menu (1 to 3), in their number keys' order; empty for another number. */
[[nodiscard]] SHOOTERGAME_API TArrayView<const EShooterRadioMessage> GetRadioMenuMessages(int32 Menu);

/**
 * The radio menu a message is in (1 to 3), or 0 for the two a player sends by itself ("Fire in the hole!", "Bomb has
 * been planted."): what the radio's sound tells apart (AShooterPlayerController::GetRadioSound).
 */
[[nodiscard]] SHOOTERGAME_API int32 GetRadioMessageMenu(EShooterRadioMessage Message);

/** A radio menu's title ("Radio Commands", "Group Radio Commands", "Radio Responses/Reports"). */
[[nodiscard]] SHOOTERGAME_API const TCHAR* GetRadioMenuTitle(int32 Menu);

/**
 * A message that asks the team for something (radio1's and radio2's, and "Need backup."): a bot of the team answers
 * it ("Affirmative.", or "Reporting in." to "Report in, team.").
 */
[[nodiscard]] SHOOTERGAME_API bool IsRadioRequest(EShooterRadioMessage Message);

/** The other team (CT for T, T for CT, None for None). */
[[nodiscard]] SHOOTERGAME_API EShooterTeam GetOpposingTeam(EShooterTeam Team);

/** Counter-Strike's message for a round's end ("Target Successfully Bombed!", ...). */
[[nodiscard]] SHOOTERGAME_API const TCHAR* GetRoundEndMessage(EShooterRoundEndReason Reason);

/** The team a round end reason gives the round to (None for a draw). */
[[nodiscard]] SHOOTERGAME_API EShooterTeam GetRoundEndWinner(EShooterRoundEndReason Reason);

/**
 * A team's tag: the PlayerStartTag of its starts and the tag of its buy zone in de_leon (plan decision D15: the map
 * carries the game's meaning as tags). NAME_None for None.
 */
[[nodiscard]] SHOOTERGAME_API FName GetShooterTeamTag(EShooterTeam Team);

/** The team a name stands for ("CT" or "T", any case), None otherwise. */
[[nodiscard]] SHOOTERGAME_API EShooterTeam ParseShooterTeam(const FString& Text);

/** The team's display name ("CT", "T", "None"). */
[[nodiscard]] SHOOTERGAME_API const TCHAR* GetShooterTeamName(EShooterTeam Team);

/** The team menu's choices (CS: jointeam): a side, the automatic choice (the smaller team) or spectating. */
enum class EShooterTeamChoice : uint8
{
	CT,
	T,
	Auto,
	Spectate,
};

/** The choice a name stands for ("CT", "T", "Auto", "Spectate", any case); false for another text. */
[[nodiscard]] SHOOTERGAME_API bool ParseShooterTeamChoice(const FString& Text, EShooterTeamChoice& OutChoice);

/** The choice's name ("CT", "T", "Auto", "Spectate"). */
[[nodiscard]] SHOOTERGAME_API const TCHAR* GetShooterTeamChoiceName(EShooterTeamChoice Choice);

/**
 * The bots' skill (ps2-polish P9; CS: bot_difficulty 0 easy, 1 normal, 2 hard, 3 expert): each is a preset of the
 * bots' reaction, aim, turn rate, recoil control and memory (FShooterBotSkill,
 * AShooterAIController::DifficultyPresets).
 */
UENUM()
enum class EShooterBotDifficulty : uint8
{
	Easy,
	Normal,
	Hard,
	Expert,
};

/** The difficulty's name ("Easy", "Normal", "Hard", "Expert"): the menu's and the URL's (`?difficulty=Hard`). */
[[nodiscard]] SHOOTERGAME_API const TCHAR* GetBotDifficultyName(EShooterBotDifficulty Difficulty);

/** The difficulty a name stands for (any case), or CS's bot_difficulty number (0 to 3); false for another text. */
[[nodiscard]] SHOOTERGAME_API bool ParseBotDifficulty(const FString& Text, EShooterBotDifficulty& OutDifficulty);

/**
 * A bot's skill at a difficulty (ps2-polish P9; CS's bot profiles, whose difficulty templates set the reaction time
 * and the aim): AShooterAIController::ApplyDifficulty copies it into a new bot. DefaultGame.ini's `+DifficultyPresets`
 * of [/Script/ShooterGame.ShooterAIController] hold one a difficulty.
 */
USTRUCT()
struct SHOOTERGAME_API FShooterBotSkill
{
	GENERATED_BODY()

	/** The difficulty the preset is for. */
	UPROPERTY()
	EShooterBotDifficulty Difficulty = EShooterBotDifficulty::Normal;

	/** Seconds from an enemy coming into sight to the first shot (AShooterAIController::ReactionTime). */
	UPROPERTY()
	float ReactionTime = 0.35f;

	/** The aim's error on a new target (degrees), how fast it settles (seconds for 1/e) and its floor. */
	UPROPERTY()
	float AimError = 5.0f;

	UPROPERTY()
	float AimErrorDecayTime = 0.8f;

	UPROPERTY()
	float MinAimError = 0.4f;

	/** How fast the bot turns to its aim, degrees a second. */
	UPROPERTY()
	float AimTurnRate = 360.0f;

	/** The part of each recoil kick the bot pulls back down (0 to 1). */
	UPROPERTY()
	float RecoilCompensation = 0.5f;

	/** Seconds an enemy out of sight is remembered. */
	UPROPERTY()
	float EnemyMemory = 1.5f;
};

/**
 * A match's setup (ps2-polish P9): the main menu's choices, saved with the player's options (UShooterPersistentUser),
 * that travel to the match as URL options (`?bots=9?difficulty=Normal?winrounds=3`, GetURLOptions) which
 * AShooterGameMode::InitGame reads.
 */
struct SHOOTERGAME_API FShooterMatchSettings
{
	/** The fewest and the most bots the menu offers: 10 players at most, the PS2's budget (the player counts). */
	static constexpr int32 MinBots = 1;
	static constexpr int32 MaxBots = 9;

	/** The map (a long package name, `/Game/Maps/de_leon`). */
	FString MapName;
	/** The bots' skill. */
	EShooterBotDifficulty BotDifficulty = EShooterBotDifficulty::Normal;
	/** The rounds a team needs to win (3: a best of 5). */
	int32 RoundsToWin = 3;
	/** The bots in the match, shared between the teams around the player (AShooterGameMode::RebalanceBots). */
	int32 NumBots = MaxBots;

	/** The menu's choices of rounds to win (CS's best of 5, 9 and 15, and mp_maxrounds 30's 16). */
	[[nodiscard]] static TArrayView<const int32> GetRoundsToWinChoices();

	/** The rounds a match of RoundsToWin lasts at most: 2 N - 1 (the first team to N wins). */
	[[nodiscard]] static int32 GetMaxRounds(int32 InRoundsToWin)
	{
		return (2 * FMath::Max(1, InRoundsToWin)) - 1;
	}

	/** The URL options of the match, without the leading '?': `bots=9?difficulty=Normal?winrounds=3`. */
	[[nodiscard]] FString GetURLOptions() const;
};
