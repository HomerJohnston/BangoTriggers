#include "BangoTriggerConstantCustomization.h"

#include "BangoTriggerConstant.h"
#include "BangoValueWidgets.h"
#include "DetailWidgetRow.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "BangoTriggerConstantCustomization"

TSharedRef<IPropertyTypeCustomization> FBangoTriggerConstantCustomization::MakeInstance()
{
	return MakeShared<FBangoTriggerConstantCustomization>();
}

void FBangoTriggerConstantCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	NameHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FBangoTriggerConstant, Name));
	TypeHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FBangoTriggerConstant, Type));
	ValueHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FBangoTriggerConstant, Value));
	ActorHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FBangoTriggerConstant, Actor));
	if (!NameHandle || !TypeHandle || !ValueHandle || !ActorHandle)
	{
		return;
	}

	// The name on the left, where an array element's index would be; type and value on the right. The details panel
	// adds the array's own buttons (insert, delete, duplicate) after them.
	HeaderRow
	.NameContent()
	[
		SNew(SEditableTextBox)
		.Font(CustomizationUtils.GetRegularFont())
		.MinDesiredWidth(100.0f)
		.Text_Lambda([this]()
		{
			FString Name;
			return NameHandle->GetValue(Name) == FPropertyAccess::MultipleValues
				? LOCTEXT("MultipleNames", "Multiple Values")
				: FText::FromString(Name);
		})
		.OnTextCommitted_Lambda([this](const FText& NewText, ETextCommit::Type)
		{
			const FString NewName = NewText.ToString().TrimStartAndEnd();
			if (NewName != GetString(NameHandle))
			{
				NameHandle->SetValue(NewName);
			}
		})
	]
	.ValueContent()
	.MinDesiredWidth(250.0f)
	.MaxDesiredWidth(600.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 4.0f, 0.0f)
		[
			SNew(SBangoTypePicker)
			.ShowTypeName(true)
			.Type_Lambda([this]() { return GetString(TypeHandle); })
			.OnTypePicked_Lambda([this](const FString& NewType) { SetType(NewType); })
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SWidgetSwitcher)
			.WidgetIndex_Lambda([this]() { return IsActorType() ? 1 : 0; })
			+ SWidgetSwitcher::Slot()
			[
				SNew(SBangoValueBox)
				.Type_Lambda([this]() { return GetString(TypeHandle); })
				.Value_Lambda([this]() { return GetString(ValueHandle); })
				.HintText(LOCTEXT("NoValue", "nil"))
				.OnValueCommitted_Lambda([this](const FString& BangoText)
				{
					if (BangoText != GetString(ValueHandle))
					{
						ValueHandle->SetValue(BangoText);
					}
				})
			]
			+ SWidgetSwitcher::Slot()
			[
				// The engine's actor picker (and eyedropper), for actors in the same level.
				ActorHandle->CreatePropertyValueWidget()
			]
		]
	];
}

FString FBangoTriggerConstantCustomization::GetString(const TSharedPtr<IPropertyHandle>& Handle) const
{
	FString Value;
	// With several triggers selected whose values differ: empty.
	if (Handle && Handle->GetValue(Value) == FPropertyAccess::Success)
	{
		return Value;
	}
	return FString();
}

bool FBangoTriggerConstantCustomization::IsActorType() const
{
	FBangoTriggerConstant Probe;
	Probe.Type = GetString(TypeHandle);
	return Probe.IsActorType();
}

void FBangoTriggerConstantCustomization::SetType(const FString& NewType)
{
	if (NewType == GetString(TypeHandle))
	{
		return;
	}
	// One undoable change: the type, and the value cleared as SCRIPT VARS clears it (nil stays). The name isn't touched.
	const FScopedTransaction Transaction(LOCTEXT("ChangeConstantType", "Change Constant Type"));
	const FString OldValue = GetString(ValueHandle);
	TypeHandle->SetValue(NewType);
	ValueHandle->SetValue(SBangoValueBox::GetValueAfterTypeChange(OldValue));
	FBangoTriggerConstant Probe;
	Probe.Type = NewType;
	if (!Probe.IsActorType())
	{
		const UObject* NoActor = nullptr;
		ActorHandle->SetValue(NoActor);
	}
}

#undef LOCTEXT_NAMESPACE
