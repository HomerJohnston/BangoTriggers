#include "BangoTriggerSubsystem.h"

#include "BangoTriggerComponent.h"
#include "Engine/World.h"

UBangoTriggerSubsystem* UBangoTriggerSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UBangoTriggerSubsystem>() : nullptr;
}

void UBangoTriggerSubsystem::RegisterTrigger(UBangoTriggerComponent* Trigger)
{
	if (Trigger)
	{
		Triggers.AddUnique(Trigger);
	}
}

void UBangoTriggerSubsystem::UnregisterTrigger(UBangoTriggerComponent* Trigger)
{
	// Only cleared here: Tick may be walking the list, and skips (then removes) cleared entries.
	for (TWeakObjectPtr<UBangoTriggerComponent>& Entry : Triggers)
	{
		if (Entry.Get() == Trigger)
		{
			Entry.Reset();
		}
	}
}

void UBangoTriggerSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();

	// By index, since a trigger's scripts can add or remove triggers while this runs. New ones wait for the next frame.
	const int32 Num = Triggers.Num();
	for (int32 Index = 0; Index < Num && Index < Triggers.Num(); ++Index)
	{
		if (UBangoTriggerComponent* Trigger = Triggers[Index].Get())
		{
			Trigger->TickTrigger(Now);
		}
	}

	Triggers.RemoveAll([](const TWeakObjectPtr<UBangoTriggerComponent>& Entry) { return !Entry.IsValid(); });
}

TStatId UBangoTriggerSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBangoTriggerSubsystem, STATGROUP_Tickables);
}

bool UBangoTriggerSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}
