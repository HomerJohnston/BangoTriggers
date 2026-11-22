#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

class IPropertyHandle;
class UEnum;

/**
 * Shows a trigger's Start State (EBangoTriggerStartState) as a segmented control, a segment per value in the enum's
 * order: Deactivated, then Undetermined (yellow when picked: it will run a script as play begins). Registered for the
 * enum itself, so the property needs no wrapper. It reads the enum's values, display names and tooltips, so it would
 * suit any enum.
 */
class FBangoTriggerStartStateCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override {}

private:
	TSharedPtr<IPropertyHandle> Handle;
	const UEnum* Enum = nullptr;
	int32 NumValues = 0;

	/** The value's index in the enum, or INDEX_NONE with several triggers selected whose values differ. */
	int32 GetIndex() const;
	void SetIndex(int32 Index);
};
