#include "Engine/GameInstance.h"

#include "Engine/GameEngine.h"
#include "Level/LeonLevelFormat.h"
#include "Level/LevelLoader.h"
#include "Misc/CString.h"

#include <filesystem>
#include <iostream>

namespace
{

	[[nodiscard]] bool TravelViaSiblingLevels(
		UGameEngine& Engine, std::string_view LevelKey, std::string_view HintLevelPath)
	{
		if (LevelKey.empty() || HintLevelPath.empty())
		{
			return false;
		}
		namespace fs = std::filesystem;
		const fs::path Hint(HintLevelPath);
		const fs::path LevelsDir = Hint.parent_path();
		if (LevelsDir.empty())
		{
			return false;
		}

		const std::string Needle = FCString::ToLower(LevelKey);
		std::error_code Ec;
		for (const auto& Entry : fs::directory_iterator(LevelsDir, Ec))
		{
			if (Ec || !Entry.is_regular_file())
			{
				continue;
			}
			const fs::path Path = Entry.path();
			if (FCString::ToLower(Path.extension().string()) != ".llev")
			{
				continue;
			}
			const std::string Stem = FCString::ToLower(Path.stem().string());
			bool bMatch = (Stem == Needle) || (FCString::ToLower(Path.filename().string()) == Needle);
			if (!bMatch)
			{
				FLevelDocument Doc;
				if (LoadLeonLevelFile(Path.string(), Doc) && FCString::ToLower(Doc.Name) == Needle)
				{
					bMatch = true;
				}
			}
			if (!bMatch)
			{
				continue;
			}
			return LoadLevelFile(Engine, Path.lexically_normal().string());
		}
		return false;
	}

} // namespace

UGameInstance::UGameInstance()
	: NetDriver(std::make_unique<UNetDriver>())
{
}

UGameInstance::~UGameInstance()
{
	Shutdown();
}

void UGameInstance::Init()
{
}

void UGameInstance::Shutdown()
{
	CloseNetSession();
	LevelTravelFn = {};
	LevelBrowserVisibleFn = {};
}

bool UGameInstance::HostListen(std::uint16_t Port)
{
	return NetDriver->StartHost(Port);
}

bool UGameInstance::HostDedicated(std::uint16_t Port)
{
	return NetDriver->StartDedicated(Port);
}

bool UGameInstance::Join(const std::string& Address, std::uint16_t Port)
{
	return NetDriver->Connect(Address, Port);
}

void UGameInstance::CloseNetSession()
{
	if (NetDriver)
	{
		NetDriver->Shutdown();
	}
}

bool UGameInstance::TravelInternal(UGameEngine& Engine, std::string_view LevelKey, std::string_view HintLevelPath)
{
	if (LevelKey.empty())
	{
		return false;
	}
	if (LevelTravelFn && LevelTravelFn(Engine, LevelKey))
	{
		return true;
	}
	if (TravelViaSiblingLevels(Engine, LevelKey, HintLevelPath))
	{
		return true;
	}
	std::cerr << "GameInstance: travel failed for map '" << LevelKey << "'\n";
	return false;
}

bool UGameInstance::ServerTravel(UGameEngine& Engine, std::string_view MapName, std::string_view HintLevelPath)
{
	return TravelInternal(Engine, MapName, HintLevelPath);
}

bool UGameInstance::ClientTravel(UGameEngine& Engine, std::string_view MapName, std::string_view HintLevelPath)
{
	return TravelInternal(Engine, MapName, HintLevelPath);
}
