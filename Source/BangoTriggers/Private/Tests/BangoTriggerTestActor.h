#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BangoTriggerTestActor.generated.h"

class UBangoTriggerComponent;

/** For the Triggers.Play tests: an actor with a root component, so it can be moved. */
UCLASS(NotPlaceable, Transient, HideDropdown)
class ABangoTriggerTestActor : public AActor
{
	GENERATED_BODY()

public:
	ABangoTriggerTestActor();
};

/**
 * For the Triggers.Play tests: writes down each trigger event, as "In:a,b" (a destroyed actor is "nil"). An Undetermined
 * trigger's first state is "Activated(initial):a" or "Deactivated(initial):".
 */
UCLASS(Transient)
class UBangoTriggerTestListener : public UObject
{
	GENERATED_BODY()

public:
	TArray<FString> Events;

	void Listen(UBangoTriggerComponent* Trigger);

	/** The events so far, joined with "|", and forgets them. */
	FString Take();

	UFUNCTION()
	void OnIn(UBangoTriggerComponent* Trigger, const TArray<AActor*>& Activators);

	UFUNCTION()
	void OnOut(UBangoTriggerComponent* Trigger, const TArray<AActor*>& Activators);

	UFUNCTION()
	void OnActivated(UBangoTriggerComponent* Trigger, const TArray<AActor*>& Activators, bool bInitial);

	UFUNCTION()
	void OnDeactivated(UBangoTriggerComponent* Trigger, const TArray<AActor*>& Activators, bool bInitial);

private:
	void Add(const TCHAR* What, const TArray<AActor*>& Activators);
};
