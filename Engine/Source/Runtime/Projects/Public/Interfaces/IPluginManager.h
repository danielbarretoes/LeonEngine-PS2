#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "PluginDescriptor.h"
#include "Templates/SharedPointer.h"

/** Where a plugin lives (UE: EPluginType). */
enum class EPluginType
{
	Engine,
	Project,
	External
};

/** A discovered plugin (UE: IPlugin). */
class IPlugin
{
public:
	virtual ~IPlugin() = default;

	virtual const FString& GetName() const = 0;
	virtual const FString& GetFriendlyName() const = 0;

	/** Full path of the .lplugin file. */
	virtual const FString& GetDescriptorFileName() const = 0;

	/** Folder of the .lplugin file. */
	virtual FString GetBaseDir() const = 0;
	virtual FString GetContentDir() const = 0;

	/** "/<Name>/": where its content mounts in package paths (UE: GetMountedAssetPath). */
	virtual FString GetMountedAssetPath() const = 0;

	virtual EPluginType GetType() const = 0;
	virtual bool IsEnabled() const = 0;
	virtual bool CanContainContent() const = 0;
	virtual const FPluginDescriptor& GetDescriptor() const = 0;
};

/**
 * Finds the .lplugin files under Engine/Plugins and <Project>/Plugins and decides which are enabled: the project's
 * "Plugins" references first, then the plugin's EnabledByDefault (UE: IPluginManager). Leon links modules statically,
 * so LeonBuildTool already compiled exactly the enabled plugins' modules; this is the run-time view of that.
 */
class PROJECTS_API IPluginManager
{
public:
	virtual ~IPluginManager() = default;

	/** Scans the plugin folders again (UE: RefreshPluginsList). */
	virtual void RefreshPluginsList() = 0;

	virtual TSharedPtr<IPlugin> FindPlugin(const FString& Name) = 0;
	virtual TArray<TSharedRef<IPlugin>> GetEnabledPlugins() = 0;
	virtual TArray<TSharedRef<IPlugin>> GetEnabledPluginsWithContent() const = 0;
	virtual TArray<TSharedRef<IPlugin>> GetDiscoveredPlugins() = 0;

	static IPluginManager& Get();
};
