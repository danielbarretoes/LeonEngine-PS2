#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UObject/Object.h"
#include "GeneralProjectSettings.generated.h"

/**
 * The project's description (UE: UGeneralProjectSettings), read from [/Script/EngineSettings.GeneralProjectSettings] of
 * the Game config (the project's DefaultGame.ini).
 */
UCLASS(Config = Game, DefaultConfig)
class ENGINESETTINGS_API UGeneralProjectSettings : public UObject
{
	GENERATED_BODY()

public:
	UGeneralProjectSettings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The company or person that makes the project (UE: CompanyName). */
	UPROPERTY(Config)
	FString CompanyName;

	/** The copyright notice (UE: CopyrightNotice). */
	UPROPERTY(Config)
	FString CopyrightNotice;

	/** What the project is (UE: Description). */
	UPROPERTY(Config)
	FString Description;

	/** The project's web page (UE: Homepage). */
	UPROPERTY(Config)
	FString Homepage;

	/** The project's license (UE: LicensingTerms). */
	UPROPERTY(Config)
	FString LicensingTerms;

	/** The project's unique id (UE: ProjectID). */
	UPROPERTY(Config)
	FGuid ProjectID;

	/** The project's name (UE: ProjectName). */
	UPROPERTY(Config)
	FString ProjectName;

	/** The project's version (UE: ProjectVersion). */
	UPROPERTY(Config)
	FString ProjectVersion;

	/** Who to contact about the project (UE: SupportContact). */
	UPROPERTY(Config)
	FString SupportContact;
};
