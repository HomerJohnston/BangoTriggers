#include "BangoTriggersEditor.h"

#include "BangoTriggerShapeComponent.h"
#include "BangoTriggerShapeVisualizer.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"

void FBangoTriggersEditorModule::StartupModule()
{
	if (GUnrealEd)
	{
		const TSharedPtr<FComponentVisualizer> Visualizer = MakeShared<FBangoTriggerShapeVisualizer>();
		GUnrealEd->RegisterComponentVisualizer(UBangoTriggerShapeComponent::StaticClass()->GetFName(), Visualizer);
		Visualizer->OnRegister();
	}
}

void FBangoTriggersEditorModule::ShutdownModule()
{
	if (GUnrealEd)
	{
		GUnrealEd->UnregisterComponentVisualizer(UBangoTriggerShapeComponent::StaticClass()->GetFName());
	}
}

IMPLEMENT_MODULE(FBangoTriggersEditorModule, BangoTriggersEditor)
