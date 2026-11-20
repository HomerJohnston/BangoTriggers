#pragma once

#include "CoreMinimal.h"
#include "BangoInlineScript.h"
#include "BangoScriptSubsystem.h"
#include "BangoTypes.h"

/**
 * One of a trigger's event scripts (On Activator In and Out, On Activated, On Deactivated). Checked when play begins,
 * to show errors early; that compiles it into Bango's inline script cache, so spawning it doesn't compile it again.
 */
class BANGOTRIGGERS_API FBangoTriggerScript
{
public:
	/** Owner names it, in errors and the debugger ("Trigger_1 (inline script)"). Shows any error; a script that doesn't compile never runs. */
	void Prepare(const FBangoInlineScript& InScript, const UObject* InOwner);

	/** Starts the script with Args (its "Activation" inputs, in order), if there is one and it compiled. */
	void Spawn(UBangoScriptSubsystem& Scripts, const TArray<FBangoValue>& Args) const;

private:
	FBangoInlineScript Script;
	TWeakObjectPtr<const UObject> Owner;
	bool bUsable = false;
};
