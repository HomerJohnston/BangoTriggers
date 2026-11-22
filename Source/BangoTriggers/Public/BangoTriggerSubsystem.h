#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BangoTriggerSubsystem.generated.h"

class UBangoTriggerComponent;

/** Ticks every Bango trigger in a game world, in one place. Doesn't tick while the game is paused. */
UCLASS()
class BANGOTRIGGERS_API UBangoTriggerSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UBangoTriggerSubsystem* Get(const UObject* WorldContext);

	void RegisterTrigger(UBangoTriggerComponent* Trigger);
	void UnregisterTrigger(UBangoTriggerComponent* Trigger);

	// UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	TArray<TWeakObjectPtr<UBangoTriggerComponent>> Triggers;
};
