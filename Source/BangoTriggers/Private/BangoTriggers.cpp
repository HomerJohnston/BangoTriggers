#include "BangoTriggerDebugDraw.h"
#include "Modules/ModuleManager.h"

class FBangoTriggersModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		BangoTriggerDebugDraw::Register();
	}

	virtual void ShutdownModule() override
	{
		BangoTriggerDebugDraw::Unregister();
	}
};

IMPLEMENT_MODULE(FBangoTriggersModule, BangoTriggers)
