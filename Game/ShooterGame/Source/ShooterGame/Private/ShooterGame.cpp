#include "ShooterGame.h"

#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "ShooterTypes.h"
#include "Sound/SoundWave.h"
#include "UObject/WeakObjectPtrTemplates.h"

// The game's classes are UObjects the engine finds by reflection (plan decision D18): GlobalDefaultGameMode names
// /Script/ShooterGame.ShooterGameMode, which names the pawn, the controllers, the player state and the HUD. The module
// itself does nothing at startup.
IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, ShooterGame, "ShooterGame")

DEFINE_LOG_CATEGORY(LogShooter);

FName GetShooterTeamTag(EShooterTeam Team)
{
	switch (Team)
	{
		case EShooterTeam::CT:
			return FName(TEXT("CT"));
		case EShooterTeam::T:
			return FName(TEXT("T"));
		case EShooterTeam::None:
			break;
	}
	return NAME_None;
}

EShooterTeam ParseShooterTeam(const FString& Text)
{
	if (Text.Equals(TEXT("CT"), ESearchCase::IgnoreCase))
	{
		return EShooterTeam::CT;
	}
	if (Text.Equals(TEXT("T"), ESearchCase::IgnoreCase))
	{
		return EShooterTeam::T;
	}
	return EShooterTeam::None;
}

bool ParseShooterTeamChoice(const FString& Text, EShooterTeamChoice& OutChoice)
{
	for (const EShooterTeamChoice Choice :
		{EShooterTeamChoice::CT, EShooterTeamChoice::T, EShooterTeamChoice::Auto, EShooterTeamChoice::Spectate})
	{
		if (Text.Equals(GetShooterTeamChoiceName(Choice), ESearchCase::IgnoreCase))
		{
			OutChoice = Choice;
			return true;
		}
	}
	return false;
}

const TCHAR* GetShooterTeamChoiceName(EShooterTeamChoice Choice)
{
	switch (Choice)
	{
		case EShooterTeamChoice::CT:
			return TEXT("CT");
		case EShooterTeamChoice::T:
			return TEXT("T");
		case EShooterTeamChoice::Auto:
			return TEXT("Auto");
		case EShooterTeamChoice::Spectate:
			break;
	}
	return TEXT("Spectate");
}

const TCHAR* GetBotDifficultyName(EShooterBotDifficulty Difficulty)
{
	switch (Difficulty)
	{
		case EShooterBotDifficulty::Easy:
			return TEXT("Easy");
		case EShooterBotDifficulty::Normal:
			return TEXT("Normal");
		case EShooterBotDifficulty::Hard:
			return TEXT("Hard");
		case EShooterBotDifficulty::Expert:
			break;
	}
	return TEXT("Expert");
}

bool ParseBotDifficulty(const FString& Text, EShooterBotDifficulty& OutDifficulty)
{
	const FString Trimmed = Text.TrimStartAndEnd();
	for (const EShooterBotDifficulty Difficulty : {EShooterBotDifficulty::Easy, EShooterBotDifficulty::Normal,
			 EShooterBotDifficulty::Hard, EShooterBotDifficulty::Expert})
	{
		// The name, or CS's bot_difficulty number (0 easy ... 3 expert).
		if (Trimmed.Equals(GetBotDifficultyName(Difficulty), ESearchCase::IgnoreCase) ||
			Trimmed == FString::Printf(TEXT("%d"), static_cast<int32>(Difficulty)))
		{
			OutDifficulty = Difficulty;
			return true;
		}
	}
	return false;
}

TArrayView<const int32> FShooterMatchSettings::GetRoundsToWinChoices()
{
	static const int32 Choices[] = {3, 5, 8, 16};
	return Choices;
}

FString FShooterMatchSettings::GetURLOptions() const
{
	return FString::Printf(TEXT("bots=%d?difficulty=%s?winrounds=%d"), NumBots, GetBotDifficultyName(BotDifficulty),
		FMath::Max(1, RoundsToWin));
}

const TCHAR* GetShooterTeamName(EShooterTeam Team)
{
	switch (Team)
	{
		case EShooterTeam::CT:
			return TEXT("CT");
		case EShooterTeam::T:
			return TEXT("T");
		case EShooterTeam::None:
			break;
	}
	return TEXT("None");
}

EShooterTeam GetOpposingTeam(EShooterTeam Team)
{
	switch (Team)
	{
		case EShooterTeam::CT:
			return EShooterTeam::T;
		case EShooterTeam::T:
			return EShooterTeam::CT;
		case EShooterTeam::None:
			break;
	}
	return EShooterTeam::None;
}

const TCHAR* GetRoundEndMessage(EShooterRoundEndReason Reason)
{
	switch (Reason)
	{
		case EShooterRoundEndReason::TargetBombed:
			return TEXT("Target Successfully Bombed!");
		case EShooterRoundEndReason::BombDefused:
			return TEXT("The bomb has been defused!");
		case EShooterRoundEndReason::CTsEliminated:
			return TEXT("Terrorists Win!");
		case EShooterRoundEndReason::TerroristsEliminated:
			return TEXT("Counter-Terrorists Win!");
		case EShooterRoundEndReason::TargetSaved:
			return TEXT("Target has been saved!");
		case EShooterRoundEndReason::Draw:
			return TEXT("Round Draw!");
		case EShooterRoundEndReason::None:
			break;
	}
	return TEXT("");
}

const TCHAR* GetBuyPlanName(EShooterBuyPlan Plan)
{
	switch (Plan)
	{
		case EShooterBuyPlan::Pistol:
			return TEXT("Pistol");
		case EShooterBuyPlan::Eco:
			return TEXT("Eco");
		case EShooterBuyPlan::Force:
			return TEXT("Force");
		case EShooterBuyPlan::Full:
			return TEXT("Full");
	}
	return TEXT("");
}

const TCHAR* GetRadioMessageText(EShooterRadioMessage Message)
{
	// CS 1.6's radio texts (its titles.txt).
	switch (Message)
	{
		case EShooterRadioMessage::CoverMe:
			return TEXT("Cover me!");
		case EShooterRadioMessage::YouTakeThePoint:
			return TEXT("You take the point.");
		case EShooterRadioMessage::HoldThisPosition:
			return TEXT("Hold this position.");
		case EShooterRadioMessage::RegroupTeam:
			return TEXT("Regroup team.");
		case EShooterRadioMessage::FollowMe:
			return TEXT("Follow me.");
		case EShooterRadioMessage::TakingFire:
			return TEXT("Taking fire, need assistance!");
		case EShooterRadioMessage::GoGoGo:
			return TEXT("Go go go!");
		case EShooterRadioMessage::TeamFallBack:
			return TEXT("Team, fall back!");
		case EShooterRadioMessage::StickTogether:
			return TEXT("Stick together, team.");
		case EShooterRadioMessage::GetInPosition:
			return TEXT("Get in position and wait for my go.");
		case EShooterRadioMessage::StormTheFront:
			return TEXT("Storm the front!");
		case EShooterRadioMessage::ReportIn:
			return TEXT("Report in, team.");
		case EShooterRadioMessage::Affirmative:
			return TEXT("Affirmative.");
		case EShooterRadioMessage::EnemySpotted:
			return TEXT("Enemy spotted.");
		case EShooterRadioMessage::NeedBackup:
			return TEXT("Need backup.");
		case EShooterRadioMessage::SectorClear:
			return TEXT("Sector clear.");
		case EShooterRadioMessage::InPosition:
			return TEXT("I'm in position.");
		case EShooterRadioMessage::ReportingIn:
			return TEXT("Reporting in.");
		case EShooterRadioMessage::GetOut:
			return TEXT("Get out of there, it's gonna blow!");
		case EShooterRadioMessage::Negative:
			return TEXT("Negative.");
		case EShooterRadioMessage::EnemyDown:
			return TEXT("Enemy down.");
		case EShooterRadioMessage::FireInTheHole:
			return TEXT("Fire in the hole!");
		case EShooterRadioMessage::BombPlanted:
			return TEXT("Bomb has been planted.");
		case EShooterRadioMessage::None:
			break;
	}
	return TEXT("");
}

TArrayView<const EShooterRadioMessage> GetRadioMenuMessages(int32 Menu)
{
	static const EShooterRadioMessage Radio1[] = {EShooterRadioMessage::CoverMe, EShooterRadioMessage::YouTakeThePoint,
		EShooterRadioMessage::HoldThisPosition, EShooterRadioMessage::RegroupTeam, EShooterRadioMessage::FollowMe,
		EShooterRadioMessage::TakingFire};
	static const EShooterRadioMessage Radio2[] = {EShooterRadioMessage::GoGoGo, EShooterRadioMessage::TeamFallBack,
		EShooterRadioMessage::StickTogether, EShooterRadioMessage::GetInPosition, EShooterRadioMessage::StormTheFront,
		EShooterRadioMessage::ReportIn};
	static const EShooterRadioMessage Radio3[] = {EShooterRadioMessage::Affirmative, EShooterRadioMessage::EnemySpotted,
		EShooterRadioMessage::NeedBackup, EShooterRadioMessage::SectorClear, EShooterRadioMessage::InPosition,
		EShooterRadioMessage::ReportingIn, EShooterRadioMessage::GetOut, EShooterRadioMessage::Negative,
		EShooterRadioMessage::EnemyDown};
	switch (Menu)
	{
		case 1:
			return Radio1;
		case 2:
			return Radio2;
		case 3:
			return Radio3;
		default:
			break;
	}
	return {};
}

int32 GetRadioMessageMenu(EShooterRadioMessage Message)
{
	for (int32 Menu = 1; Menu <= NumRadioMenus; ++Menu)
	{
		if (GetRadioMenuMessages(Menu).Contains(Message))
		{
			return Menu;
		}
	}
	return 0;
}

const TCHAR* GetRadioMenuTitle(int32 Menu)
{
	switch (Menu)
	{
		case 1:
			return TEXT("Radio Commands");
		case 2:
			return TEXT("Group Radio Commands");
		case 3:
			return TEXT("Radio Responses/Reports");
		default:
			break;
	}
	return TEXT("");
}

bool IsRadioRequest(EShooterRadioMessage Message)
{
	return (Message >= EShooterRadioMessage::CoverMe && Message <= EShooterRadioMessage::ReportIn) ||
		Message == EShooterRadioMessage::NeedBackup;
}

EShooterTeam GetRoundEndWinner(EShooterRoundEndReason Reason)
{
	switch (Reason)
	{
		case EShooterRoundEndReason::TargetBombed:
		case EShooterRoundEndReason::CTsEliminated:
			return EShooterTeam::T;
		case EShooterRoundEndReason::BombDefused:
		case EShooterRoundEndReason::TerroristsEliminated:
		case EShooterRoundEndReason::TargetSaved:
			return EShooterTeam::CT;
		case EShooterRoundEndReason::Draw:
		case EShooterRoundEndReason::None:
			break;
	}
	return EShooterTeam::None;
}

const TArray<FSoftObjectPath>& FShooterSurfaceSounds::GetPaths(EPhysicalSurface Surface) const
{
	static const TArray<FSoftObjectPath> None;
	switch (Surface)
	{
		case SHOOTER_SURFACE_Default:
			return Default;
		case SHOOTER_SURFACE_Concrete:
			return Concrete;
		case SHOOTER_SURFACE_Dirt:
			return Dirt;
		case SHOOTER_SURFACE_Metal:
			return Metal;
		case SHOOTER_SURFACE_Wood:
			return Wood;
		case SHOOTER_SURFACE_Tile:
			return Tile;
		case SHOOTER_SURFACE_Glass:
			return Glass;
		case SHOOTER_SURFACE_Computer:
			return Computer;
		case SHOOTER_SURFACE_Flesh:
			return Flesh;
		default:
			break;
	}
	return None;
}

UObject* LoadShooterObject(const FSoftObjectPath& Path)
{
	if (Path.IsNull())
	{
		return nullptr;
	}
	// The paths resolved before (UE: a TSoftObjectPtr keeps what it resolved; every spawn asks for the same few dozen
	// assets): a weak pointer, so an asset the collector freed is looked for again.
	static TMap<FSoftObjectPath, TWeakObjectPtr<UObject>> Resolved;
	if (const TWeakObjectPtr<UObject>* Known = Resolved.Find(Path))
	{
		if (UObject* Object = Known->Get())
		{
			return Object;
		}
	}
	// In memory (the preload): a lookup. Else the file system says whether the art exists before a load warns.
	UObject* Object = Path.ResolveObject();
	if (Object == nullptr && FPackageName::DoesPackageExist(Path.GetLongPackageName()))
	{
		Object = Path.TryLoad();
	}
	if (Object != nullptr)
	{
		Resolved.Add(Path, Object);
	}
	return Object;
}

USoundWave* LoadShooterSound(const FSoftObjectPath& Path)
{
	return LoadShooterAsset<USoundWave>(Path);
}

void FShooterSurfaceSoundSet::Load(const FShooterSurfaceSounds& Paths)
{
	Sounds.Init(nullptr, NumSurfaces * MaxVariants);
	for (int32 Surface = 0; Surface < NumSurfaces; ++Surface)
	{
		const TArray<FSoftObjectPath>& Variants = Paths.GetPaths(static_cast<EPhysicalSurface>(Surface));
		int32 Slot = 0;
		for (const FSoftObjectPath& Path : Variants)
		{
			USoundWave* Sound = LoadShooterSound(Path);
			if (Sound != nullptr && Slot < MaxVariants)
			{
				Sounds[(Surface * MaxVariants) + Slot++] = Sound;
			}
		}
	}
}

int32 FShooterSurfaceSoundSet::GetNumVariants(EPhysicalSurface Surface) const
{
	const int32 Index = static_cast<int32>(Surface);
	if (Index < 0 || Index >= NumSurfaces || Sounds.Num() != NumSurfaces * MaxVariants)
	{
		return 0;
	}
	int32 Count = 0;
	while (Count < MaxVariants && Sounds[(Index * MaxVariants) + Count] != nullptr)
	{
		++Count;
	}
	return Count;
}

USoundWave* FShooterSurfaceSoundSet::Get(EPhysicalSurface Surface, int32 Variant) const
{
	EPhysicalSurface Source = Surface;
	int32 Count = GetNumVariants(Source);
	if (Count == 0)
	{
		Source = SHOOTER_SURFACE_Default;
		Count = GetNumVariants(Source);
	}
	if (Count == 0)
	{
		return nullptr;
	}
	return Sounds[(static_cast<int32>(Source) * MaxVariants) + (FMath::Abs(Variant) % Count)];
}
