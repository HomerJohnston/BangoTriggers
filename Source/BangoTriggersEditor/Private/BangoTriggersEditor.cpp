#include "BangoTriggersEditor.h"

#include "BangoTriggerComponent.h"
#include "BangoTriggerConstantCustomization.h"
#include "BangoTriggerShapeVisualizer.h"
#include "BangoTriggerStartStateCustomization.h"
#include "Editor/UnrealEdEngine.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "UnrealEdGlobals.h"

namespace
{
	const FName ConstantStructName(TEXT("BangoTriggerConstant"));
	// An enum's customization is found by the enum's own name.
	const FName StartStateEnumName(TEXT("EBangoTriggerStartState"));
}

void FBangoTriggersEditorModule::StartupModule()
{
	if (GUnrealEd)
	{
		const TSharedPtr<FComponentVisualizer> Visualizer = MakeShared<FBangoTriggerShapeVisualizer>();
		GUnrealEd->RegisterComponentVisualizer(UBangoTriggerComponent::StaticClass()->GetFName(), Visualizer);
		Visualizer->OnRegister();
	}

	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	PropertyEditor.RegisterCustomPropertyTypeLayout(ConstantStructName,
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FBangoTriggerConstantCustomization::MakeInstance));
	PropertyEditor.RegisterCustomPropertyTypeLayout(StartStateEnumName,
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FBangoTriggerStartStateCustomization::MakeInstance));
	PropertyEditor.NotifyCustomizationModuleChanged();
}

void FBangoTriggersEditorModule::ShutdownModule()
{
	if (GUnrealEd)
	{
		GUnrealEd->UnregisterComponentVisualizer(UBangoTriggerComponent::StaticClass()->GetFName());
	}
	if (FPropertyEditorModule* PropertyEditor = FModuleManager::GetModulePtr<FPropertyEditorModule>(TEXT("PropertyEditor")))
	{
		PropertyEditor->UnregisterCustomPropertyTypeLayout(ConstantStructName);
		PropertyEditor->UnregisterCustomPropertyTypeLayout(StartStateEnumName);
	}
}

IMPLEMENT_MODULE(FBangoTriggersEditorModule, BangoTriggersEditor)
