#pragma once

#include "Modules/ModuleManager.h"

/** Editor tools for Bango Triggers: editing a trigger's regions in the level viewport, and its Constants' rows. */
class FBangoTriggersEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
