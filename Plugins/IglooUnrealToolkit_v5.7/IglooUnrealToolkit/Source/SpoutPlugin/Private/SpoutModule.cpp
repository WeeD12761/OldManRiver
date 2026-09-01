// Some copyright should be here...

#include "SpoutModule.h"
#include "SpoutPluginPrivatePCH.h"

#include "Core.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
//#include "IPluginManager.h"

DEFINE_LOG_CATEGORY(SpoutUELog);

#define LOCTEXT_NAMESPACE "FSpoutModule"

void FSpoutModule::StartupModule()
{

	UE_LOG(SpoutUELog, Warning, TEXT("Spout Module Starting"));

	// Maps virtual shader source directory to the plugin's actual shaders directory.
	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("IglooUnrealToolkit"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/SpoutShaders"), PluginShaderDir);
}

	
IMPLEMENT_MODULE(FSpoutModule, SpoutPlugin)

#undef LOCTEXT_NAMESPACE