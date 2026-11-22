#include "BangoTriggerStartStateCustomization.h"

#include "BangoTriggerTypes.h"
#include "DetailWidgetRow.h"
#include "PropertyHandle.h"
#include "Styling/AppStyle.h"
#include "Styling/SegmentedControlStyle.h"
#include "UObject/Class.h"
#include "UObject/EnumProperty.h"
#include "UObject/UnrealType.h"
#include "Widgets/Input/SSegmentedControl.h"

namespace
{
	const UEnum* GetEnum(const IPropertyHandle& Handle)
	{
		const FProperty* Property = Handle.GetProperty();
		if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			return EnumProperty->GetEnum();
		}
		if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
		{
			return ByteProperty->Enum;
		}
		return nullptr;
	}

	/** A value's colour when it's picked, if it isn't the usual one: Undetermined is yellow. */
	TOptional<FLinearColor> GetPickedColor(const UEnum& Enum, int32 Index)
	{
		if (&Enum == StaticEnum<EBangoTriggerStartState>() && Enum.GetValueByIndex(Index) == (int64)EBangoTriggerStartState::Undetermined)
		{
			return FLinearColor(0.95f, 0.75f, 0.05f);
		}
		return {};
	}

	void TintPicked(FCheckBoxStyle& Style, const FLinearColor& Color)
	{
		Style.CheckedImage.TintColor = Color;
		Style.CheckedHoveredImage.TintColor = Color * 1.15f;
		Style.CheckedPressedImage.TintColor = Color * 0.85f;
		// Dark text, readable on yellow.
		const FSlateColor Text(FLinearColor(0.02f, 0.02f, 0.02f));
		Style.CheckedForeground = Text;
		Style.CheckedHoveredForeground = Text;
		Style.CheckedPressedForeground = Text;
	}

	/**
	 * The editor's segmented control style, with a picked value's own colour. A segmented control has one style, with
	 * a look for its first, middle and last segments, so a value is coloured by its place (all middle ones alike).
	 * Kept for good: the widgets point at it.
	 */
	const FSegmentedControlStyle& GetStyle(const UEnum& Enum, int32 NumValues)
	{
		static TMap<const UEnum*, TUniquePtr<FSegmentedControlStyle>> Styles;
		if (const TUniquePtr<FSegmentedControlStyle>* Found = Styles.Find(&Enum))
		{
			return **Found;
		}
		TUniquePtr<FSegmentedControlStyle> Style = MakeUnique<FSegmentedControlStyle>(FAppStyle::Get().GetWidgetStyle<FSegmentedControlStyle>("SegmentedControl"));
		for (int32 Index = 0; Index < NumValues; ++Index)
		{
			if (const TOptional<FLinearColor> Color = GetPickedColor(Enum, Index))
			{
				FCheckBoxStyle& Segment = Index == 0 ? Style->FirstControlStyle
					: Index == NumValues - 1 ? Style->LastControlStyle
					: Style->ControlStyle;
				TintPicked(Segment, *Color);
			}
		}
		return *Styles.Add(&Enum, MoveTemp(Style));
	}
}

TSharedRef<IPropertyTypeCustomization> FBangoTriggerStartStateCustomization::MakeInstance()
{
	return MakeShared<FBangoTriggerStartStateCustomization>();
}

void FBangoTriggerStartStateCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	Handle = PropertyHandle;
	Enum = GetEnum(*PropertyHandle);
	// The enum's own values, without the hidden _MAX a UENUM gets.
	NumValues = Enum ? Enum->NumEnums() - (Enum->ContainsExistingMax() ? 1 : 0) : 0;
	if (NumValues <= 0)
	{
		HeaderRow
		.NameContent()[PropertyHandle->CreatePropertyNameWidget()]
		.ValueContent()[PropertyHandle->CreatePropertyValueWidget()];
		return;
	}

	// With several triggers selected whose values differ, no segment is picked.
	TSharedRef<SSegmentedControl<int32>> Control = SNew(SSegmentedControl<int32>)
		.Style(&GetStyle(*Enum, NumValues))
		.Value_Lambda([this]() { return GetIndex(); })
		.OnValueChanged_Lambda([this](int32 Index) { SetIndex(Index); });
	for (int32 Index = 0; Index < NumValues; ++Index)
	{
		Control->AddSlot(Index)
			.Text(Enum->GetDisplayNameTextByIndex(Index))
			.ToolTip(Enum->GetToolTipTextByIndex(Index));
	}

	HeaderRow
	.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(220.0f)
	[
		Control
	];
}

int32 FBangoTriggerStartStateCustomization::GetIndex() const
{
	FString Name;
	if (!Handle || !Enum || Handle->GetValueAsFormattedString(Name) != FPropertyAccess::Success)
	{
		return INDEX_NONE;
	}
	// The formatted value is the short name ("Undetermined"), as GetNameStringByIndex gives it.
	for (int32 Index = 0; Index < NumValues; ++Index)
	{
		if (Enum->GetNameStringByIndex(Index) == Name)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void FBangoTriggerStartStateCustomization::SetIndex(int32 Index)
{
	// One undoable change, only when it changes.
	if (Handle && Enum && Index >= 0 && Index < NumValues && Index != GetIndex())
	{
		Handle->SetValueFromFormattedString(Enum->GetNameStringByIndex(Index));
	}
}
