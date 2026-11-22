#pragma once

#include "CoreMinimal.h"
#include "BangoTypes.h"
#include "BangoTriggerConstant.generated.h"

class AActor;

/**
 * One of a trigger's own constants (spec section 8, Constants): a named value that every script on that trigger, and
 * its Condition, can read as an input, after the built-in ones. For values that differ from one trigger to the next and
 * that several of its scripts share, like a light trigger's `_red` and `_blue`.
 *
 * Kept as a script variable is: a name, a type and a value written as Bango text, edited on one row in the details
 * panel with the script editor's own type picker and value box (BangoTriggersEditor). An `entity` is the one exception:
 * it's an actor picked in the level, so actors without a Bango ID can be used too.
 */
USTRUCT()
struct BANGOTRIGGERS_API FBangoTriggerConstant
{
	GENERATED_BODY()

	/** The name scripts read it by. Should start with '_', like the built-in inputs (`_red`). */
	UPROPERTY(EditAnywhere, Category = "Constant")
	FString Name = TEXT("_value");

	/** Its type, as scripts write it: "int", "vector3", "entity[]" (Bango::GetValueTypeNames). */
	UPROPERTY(EditAnywhere, Category = "Constant")
	FString Type = TEXT("int");

	/**
	 * Its value as Bango text, as a script variable's starting value is written (`[1, 0, 0]`, `"hello"`, `@lamp`,
	 * `[@a, @b]`); empty is nil. Not used for an `entity`.
	 */
	UPROPERTY(EditAnywhere, Category = "Constant")
	FString Value = TEXT("0");

	/** An `entity`'s value: an actor in the same level. None is nil. */
	UPROPERTY(EditAnywhere, Category = "Constant")
	TObjectPtr<AActor> Actor;

	/** True for the type whose value is Actor. */
	bool IsActorType() const { return Type == TEXT("entity"); }

	/** How it's declared as an input: "vector3 _red". */
	FString GetDeclaration() const { return FString::Printf(TEXT("%s %s"), *Type, *Name); }

	/**
	 * Its value, for the scripts (Bango::ParseFixedValue, or the actor's entity). Worked out when the trigger begins
	 * play. False, with why in OutError, if the type or the value can't be used: OutValue is then nil.
	 */
	bool GetValue(FBangoValue& OutValue, FString& OutError) const;
};
