#pragma once

#include "CoreMinimal.h"

/**
 * A platform the cook can build content for (UE: ITargetPlatform, the part Leon's cook asks for). The cook names its
 * output folder, the platform its packages record and the config layers it reads after the target, and saves without
 * editor-only data when the target has none (every Leon target).
 */
class ITargetPlatform
{
public:
	virtual ~ITargetPlatform() = default;

	/** The name -TargetPlatform= takes and the folder under Saved/Cooked: "Win64", "PS2" (UE: PlatformName). */
	virtual FString PlatformName() const = 0;

	/** The name people read (UE: DisplayName). */
	virtual FString DisplayName() const = 0;

	/**
	 * The config platform: the Engine/Config/<Name>/ and Engine/Platforms/<Name>/Config/ layers the cook reads and
	 * stages ("Windows", "PS2") (UE: IniPlatformName).
	 */
	virtual FString IniPlatformName() const = 0;

	/**
	 * The platform a cooked package records (FPackageFileSummary::CookedPlatform) (Leon; UE keeps the cooked platform
	 * in the asset registry and names the folder "WindowsNoEditor"). The platform name by default.
	 */
	virtual FString CookedPlatformName() const
	{
		return PlatformName();
	}

	/** Whether its content keeps the editor-only data: false for a game platform (UE: HasEditorOnlyData). */
	virtual bool HasEditorOnlyData() const = 0;

	/** The byte order of its CPU (UE: IsLittleEndian). Leon's formats are little-endian; the cook refuses others. */
	virtual bool IsLittleEndian() const = 0;

	/** Whether its builds read only cooked content (UE: RequiresCookedData). */
	virtual bool RequiresCookedData() const
	{
		return true;
	}

	/** The texture formats its content uses (UE: GetAllTextureFormats). */
	virtual void GetAllTextureFormats(TArray<FName>& OutFormats) const = 0;

	/** The sound formats its content uses (UE: GetAllWaveFormats). */
	virtual void GetAllWaveFormats(TArray<FName>& OutFormats) const = 0;

	/**
	 * What the cook does not convert for this platform yet, logged once per cook; empty when its content is complete
	 * (Leon).
	 */
	virtual FString GetCookNote() const
	{
		return FString();
	}
};
