#pragma once

#include "CoreMinimal.h"

/** Persistent game session (Unreal-style UGameInstance). Survives level changes; owned by Engine. */
class ENGINE_API UGameInstance
{
public:
	UGameInstance() = default;
	virtual ~UGameInstance();

	UGameInstance(const UGameInstance&) = delete;
	UGameInstance& operator=(const UGameInstance&) = delete;
	UGameInstance(UGameInstance&&) = delete;
	UGameInstance& operator=(UGameInstance&&) = delete;

	virtual void Init();
	virtual void Shutdown();

	/** Called when a level is successfully activated for gameplay. */
	virtual void NotifyLevelOpened()
	{
		++LevelsOpened;
	}

	[[nodiscard]] int GetLevelsOpened() const
	{
		return LevelsOpened;
	}

private:
	int LevelsOpened = 0;
};
