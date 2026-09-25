#include "HAL/FileManager.h"
#include "HAL/PlatformProperties.h"
#include "Interfaces/IPluginManager.h"
#include "Interfaces/IProjectManager.h"
#include "Logging/LogMacros.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogPluginManager, Log, All);

namespace
{
	class FPlugin final : public IPlugin
	{
	public:
		FPlugin(const FString& InFileName, const FPluginDescriptor& InDescriptor, EPluginType InType)
			: Name(FPaths::GetBaseFilename(InFileName))
			, FileName(InFileName)
			, Descriptor(InDescriptor)
			, Type(InType)
		{
		}

		virtual const FString& GetName() const override
		{
			return Name;
		}
		virtual const FString& GetFriendlyName() const override
		{
			return Descriptor.FriendlyName.IsEmpty() ? Name : Descriptor.FriendlyName;
		}
		virtual const FString& GetDescriptorFileName() const override
		{
			return FileName;
		}
		virtual FString GetBaseDir() const override
		{
			return FPaths::GetPath(FileName);
		}
		virtual FString GetContentDir() const override
		{
			return GetBaseDir() / "Content";
		}
		virtual FString GetMountedAssetPath() const override
		{
			return FString("/") + Name + "/";
		}
		virtual EPluginType GetType() const override
		{
			return Type;
		}
		virtual bool IsEnabled() const override
		{
			return bEnabled;
		}
		virtual bool CanContainContent() const override
		{
			return Descriptor.bCanContainContent;
		}
		virtual const FPluginDescriptor& GetDescriptor() const override
		{
			return Descriptor;
		}

		FString Name;
		FString FileName;
		FPluginDescriptor Descriptor;
		EPluginType Type;
		bool bEnabled = false;
	};

	class FPluginManager final : public IPluginManager
	{
	public:
		virtual void RefreshPluginsList() override
		{
			Plugins.Empty();
			Discover(FPaths::EnginePluginsDir(), EPluginType::Engine);
			Discover(FPaths::ProjectPluginsDir(), EPluginType::Project);
			UpdateEnabledState();
			bScanned = true;
		}

		virtual TSharedPtr<IPlugin> FindPlugin(const FString& Name) override
		{
			ScanOnce();
			for (const TSharedRef<FPlugin>& Plugin : Plugins)
			{
				if (Plugin->GetName() == Name)
				{
					return Plugin;
				}
			}
			return nullptr;
		}

		virtual TArray<TSharedRef<IPlugin>> GetEnabledPlugins() override
		{
			ScanOnce();
			TArray<TSharedRef<IPlugin>> Result;
			for (const TSharedRef<FPlugin>& Plugin : Plugins)
			{
				if (Plugin->IsEnabled())
				{
					Result.Add(Plugin);
				}
			}
			return Result;
		}

		virtual TArray<TSharedRef<IPlugin>> GetEnabledPluginsWithContent() const override
		{
			TArray<TSharedRef<IPlugin>> Result;
			for (const TSharedRef<FPlugin>& Plugin : Plugins)
			{
				if (Plugin->IsEnabled() && Plugin->CanContainContent())
				{
					Result.Add(Plugin);
				}
			}
			return Result;
		}

		virtual TArray<TSharedRef<IPlugin>> GetDiscoveredPlugins() override
		{
			ScanOnce();
			TArray<TSharedRef<IPlugin>> Result;
			for (const TSharedRef<FPlugin>& Plugin : Plugins)
			{
				Result.Add(Plugin);
			}
			return Result;
		}

	private:
		void ScanOnce()
		{
			if (!bScanned)
			{
				RefreshPluginsList();
			}
		}

		void Discover(const FString& Directory, EPluginType Type)
		{
			if (!IFileManager::Get().DirectoryExists(*Directory))
			{
				return;
			}

			TArray<FString> Files;
			IFileManager::Get().FindFilesRecursive(
				Files, *Directory, *(FString("*.") + FPluginDescriptor::GetExtension()), true, false);
			Files.Sort();
			for (const FString& File : Files)
			{
				FPluginDescriptor Descriptor;
				FText FailReason;
				if (!Descriptor.Load(File, FailReason))
				{
					UE_LOG(LogPluginManager, Warning, "Plugin %s: %s", *File, *FailReason.ToString());
					continue;
				}
				Plugins.Add(MakeShared<FPlugin>(File, Descriptor, Type));
			}
		}

		void UpdateEnabledState()
		{
			const FProjectDescriptor* Project = IProjectManager::Get().GetCurrentProject();
			const FString Platform = FPlatformProperties::PlatformName();
			for (const TSharedRef<FPlugin>& Plugin : Plugins)
			{
				Plugin->bEnabled = Plugin->Descriptor.EnabledByDefault == EPluginEnabledByDefault::Enabled;
				if (Project != nullptr)
				{
					for (const FPluginReferenceDescriptor& Reference : Project->Plugins)
					{
						if (Reference.Name == Plugin->Name)
						{
							Plugin->bEnabled = Reference.IsEnabledForPlatform(Platform);
						}
					}
				}
			}
		}

		TArray<TSharedRef<FPlugin>> Plugins;
		bool bScanned = false;
	};
} // namespace

IPluginManager& IPluginManager::Get()
{
	static FPluginManager Singleton;
	return Singleton;
}
