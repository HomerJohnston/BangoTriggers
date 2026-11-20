#pragma once

#include "CoreMinimal.h"
#include "BangoTriggerTypes.generated.h"

/** Whether gathered actors must be inside the trigger's regions, or outside them, to be potential (spec section 4). */
UENUM(BlueprintType)
enum class EBangoTriggerPresence : uint8
{
	/** Potential if inside any region. */
	Present,
	/** Potential if outside every region. */
	NotPresent,
};

/** When the trigger is active, from how many actors are actual (spec section 6). */
UENUM(BlueprintType)
enum class EBangoTriggerRule : uint8
{
	/** Every gathered actor is actual. */
	All,
	/** At least Count actors are actual. */
	AtLeast,
	/** At most Count actors are actual (0 included). */
	AtMost,
	/** Between Min and Max actors (inclusive) are actual. */
	Between,
};

/** Where the trigger checks (spec section 9). */
UENUM(BlueprintType)
enum class EBangoTriggerRunsOn : uint8
{
	/** Standalone games, listen servers and dedicated servers. */
	Server,
	/** Only clients that aren't the server. */
	Client,
	Anywhere,
};

/** How an activation or deactivation timer behaves (spec section 9). */
UENUM(BlueprintType)
enum class EBangoTriggerTimerMode : uint8
{
	/** No timer: it happens at once. */
	None,
	/** The Rule must stay that way for the whole time, or the timer is cancelled. */
	Timeout,
	/** Once started, the timer runs to the end whatever happens. */
	Countdown,
};

UENUM(BlueprintType)
enum class EBangoTriggerState : uint8
{
	Idle,
	/** The Activation Timer is running. */
	Activating,
	Active,
	/** The Deactivation Timer is running. */
	Deactivating,
	/** No activations left: it no longer checks or runs anything. */
	Finished,
};

USTRUCT(BlueprintType)
struct BANGOTRIGGERS_API FBangoTriggerTimer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timer")
	EBangoTriggerTimerMode Mode = EBangoTriggerTimerMode::None;

	/** Seconds. The time is picked at random between Min Time and Max Time each time the timer starts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timer", meta = (ClampMin = "0", Units = "s", EditCondition = "Mode != EBangoTriggerTimerMode::None"))
	float MinTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timer", meta = (ClampMin = "0", Units = "s", EditCondition = "Mode != EBangoTriggerTimerMode::None"))
	float MaxTime = 0.0f;

	bool IsUsed() const { return Mode != EBangoTriggerTimerMode::None; }

	/** A time between MinTime and MaxTime (in either order). */
	float PickTime() const;
};
