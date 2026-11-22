#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "BangoActivatorSource.generated.h"

class UBangoTriggerComponent;

/**
 * Where a trigger gets its actors from: "who" (spec section 2). A trigger's Sources list holds these as instanced
 * structs; game modules add their own by subclassing and overriding Gather.
 */
USTRUCT()
struct BANGOTRIGGERS_API FBangoActivatorSource
{
	GENERATED_BODY()

	virtual ~FBangoActivatorSource() = default;

	/**
	 * Adds this source's actors to OutActors. It may add duplicates, the trigger's own actor or invalid actors: the
	 * trigger removes them.
	 */
	virtual void Gather(const UBangoTriggerComponent& Trigger, TArray<AActor*>& OutActors) const {}
};

/** Every actor of the chosen classes, or their subclasses. */
USTRUCT(meta = (DisplayName = "Actor Class"))
struct BANGOTRIGGERS_API FBangoActorClassSource : public FBangoActivatorSource
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Source")
	TArray<TSubclassOf<AActor>> Classes;

	virtual void Gather(const UBangoTriggerComponent& Trigger, TArray<AActor*>& OutActors) const override;
};

/** The actors with these Bango IDs, looked up in the Bango registry. IDs not registered yet give nothing. */
USTRUCT(meta = (DisplayName = "Bango IDs"))
struct BANGOTRIGGERS_API FBangoIDsSource : public FBangoActivatorSource
{
	GENERATED_BODY()

	/** Bango ID names, without the @. */
	UPROPERTY(EditAnywhere, Category = "Source")
	TArray<FString> Names;

	virtual void Gather(const UBangoTriggerComponent& Trigger, TArray<AActor*>& OutActors) const override;
};

/** Every pawn controlled by a player (on a client, only the ones it knows the controllers of: its own). */
USTRUCT(meta = (DisplayName = "Players"))
struct BANGOTRIGGERS_API FBangoPlayersSource : public FBangoActivatorSource
{
	GENERATED_BODY()

	virtual void Gather(const UBangoTriggerComponent& Trigger, TArray<AActor*>& OutActors) const override;
};

/** The pawns of this machine's local players (split-screen: several; on a server, only the host's own). */
USTRUCT(meta = (DisplayName = "Local Players"))
struct BANGOTRIGGERS_API FBangoLocalPlayersSource : public FBangoActivatorSource
{
	GENERATED_BODY()

	virtual void Gather(const UBangoTriggerComponent& Trigger, TArray<AActor*>& OutActors) const override;
};
