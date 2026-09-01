// Some copyright should be here...

#include "IglooModule.h"

#include "Core.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(IglooLog);

#define LOCTEXT_NAMESPACE "FIglooModule"

void FIglooModule::StartupModule()
{
	/// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	UE_LOG(IglooLog, Warning, TEXT("Startup Igloo Module"));
}

void FIglooModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	
	UE_LOG(IglooLog, Warning, TEXT("Shutdown Igloo Module"));
}


	
IMPLEMENT_MODULE(FIglooModule, Igloo)

#undef LOCTEXT_NAMESPACE