#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BangoTrigger.generated.h"

class UBillboardComponent;
class UBangoTriggerComponent;

/**
 * A trigger on its own, for places that aren't any one thing ("a player reaches the exit"): an actor with a Bango
 * trigger component and an editor icon. To put a trigger on a door, add the component to the door instead.
 */
UCLASS(meta = (DisplayName = "Bango Trigger"), HideCategories = (Rendering, Replication, Collision, Input, HLOD, Physics, Networking, LOD, Cooking, DataLayers, WorldPartition))
class BANGOTRIGGERS_API ABangoTrigger : public AActor
{
	GENERATED_BODY()

public:
	ABangoTrigger();

	UBangoTriggerComponent* GetTrigger() const { return Trigger; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trigger", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trigger", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBangoTriggerComponent> Trigger;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> Sprite;
#endif
};
