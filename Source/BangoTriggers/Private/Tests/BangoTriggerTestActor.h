#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BangoTriggerTestActor.generated.h"

class ABangoTrigger;

/** For the Triggers.Play tests: an actor with a root component, so it can be moved. */
UCLASS(NotPlaceable, Transient, HideDropdown)
class ABangoTriggerTestActor : public AActor
{
	GENERATED_BODY()

public:
	ABangoTriggerTestActor();
};

/** For the Triggers.Play tests: writes down each trigger event, as "In:a,b" (a destroyed actor is "nil"). */
UCLASS(Transient)
class UBangoTriggerTestListener : public UObject
{
	GENERATED_BODY()

public:
	TArray<FString> Events;

	void Listen(ABangoTrigger* Trigger);

	/** The events so far, joined with "|", and forgets them. */
	FString Take();

	UFUNCTION()
	void OnIn(ABangoTrigger* Trigger, const TArray<AActor*>& Activators);

	UFUNCTION()
	void OnOut(ABangoTrigger* Trigger, const TArray<AActor*>& Activators);

	UFUNCTION()
	void OnActivated(ABangoTrigger* Trigger, const TArray<AActor*>& Activators);

	UFUNCTION()
	void OnDeactivated(ABangoTrigger* Trigger, const TArray<AActor*>& Activators);

private:
	void Add(const TCHAR* What, const TArray<AActor*>& Activators);
};
