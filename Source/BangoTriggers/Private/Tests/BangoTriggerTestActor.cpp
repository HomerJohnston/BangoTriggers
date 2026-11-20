#include "BangoTriggerTestActor.h"

#include "BangoIDActorComponent.h"
#include "BangoTrigger.h"
#include "Components/SceneComponent.h"

ABangoTriggerTestActor::ABangoTriggerTestActor()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void UBangoTriggerTestListener::Listen(ABangoTrigger* Trigger)
{
	Trigger->OnActivatorIn.AddDynamic(this, &UBangoTriggerTestListener::OnIn);
	Trigger->OnActivatorOut.AddDynamic(this, &UBangoTriggerTestListener::OnOut);
	Trigger->OnActivated.AddDynamic(this, &UBangoTriggerTestListener::OnActivated);
	Trigger->OnDeactivated.AddDynamic(this, &UBangoTriggerTestListener::OnDeactivated);
}

FString UBangoTriggerTestListener::Take()
{
	const FString Joined = FString::Join(Events, TEXT("|"));
	Events.Reset();
	return Joined;
}

void UBangoTriggerTestListener::OnIn(ABangoTrigger* Trigger, const TArray<AActor*>& Activators)
{
	Add(TEXT("In"), Activators);
}

void UBangoTriggerTestListener::OnOut(ABangoTrigger* Trigger, const TArray<AActor*>& Activators)
{
	Add(TEXT("Out"), Activators);
}

void UBangoTriggerTestListener::OnActivated(ABangoTrigger* Trigger, const TArray<AActor*>& Activators)
{
	Add(TEXT("Activated"), Activators);
}

void UBangoTriggerTestListener::OnDeactivated(ABangoTrigger* Trigger, const TArray<AActor*>& Activators)
{
	Add(TEXT("Deactivated"), Activators);
}

void UBangoTriggerTestListener::Add(const TCHAR* What, const TArray<AActor*>& Activators)
{
	TArray<FString> Names;
	for (const AActor* Actor : Activators)
	{
		const UBangoIDActorComponent* ID = Actor ? Actor->FindComponentByClass<UBangoIDActorComponent>() : nullptr;
		Names.Add(ID ? ID->BangoName.ToString() : (Actor ? Actor->GetName() : FString(TEXT("nil"))));
	}
	Events.Add(FString::Printf(TEXT("%s:%s"), What, *FString::Join(Names, TEXT(","))));
}
