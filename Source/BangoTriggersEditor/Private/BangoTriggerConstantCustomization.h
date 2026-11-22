#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

class IPropertyHandle;

/**
 * Shows a trigger constant (FBangoTriggerConstant) on one row, as the script editor's SCRIPT VARS list shows a script
 * variable: its name, the type picker (SBangoTypePicker) and the value box (SBangoValueBox), each edited on its own.
 * Picking another type clears the value as SCRIPT VARS does, and leaves the name alone. An `entity`'s value is an actor
 * picker instead of the box, so actors without a Bango ID can be used.
 */
class FBangoTriggerConstantCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override {}

private:
	TSharedPtr<IPropertyHandle> NameHandle;
	TSharedPtr<IPropertyHandle> TypeHandle;
	TSharedPtr<IPropertyHandle> ValueHandle;
	TSharedPtr<IPropertyHandle> ActorHandle;

	FString GetString(const TSharedPtr<IPropertyHandle>& Handle) const;
	void SetType(const FString& NewType);
	bool IsActorType() const;
};
