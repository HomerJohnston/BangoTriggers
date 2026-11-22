#pragma once

#include "CoreMinimal.h"
#include "BangoTriggerTypes.h"

/**
 * The decisions a trigger makes, as plain functions with no world or actors, so they can be tested on their own
 * (Private/Tests/BangoTriggerTests.cpp). UBangoTriggerComponent calls them.
 */
namespace BangoTriggerLogic
{
	/** What the Rule looks at (spec section 6). */
	struct FRuleInput
	{
		EBangoTriggerRule Rule = EBangoTriggerRule::AtLeast;
		int32 Count = 1;
		int32 Min = 1;
		int32 Max = 1;
		int32 NumGathered = 0;
		int32 NumPotential = 0;
		int32 NumActual = 0;
		/** With no sources, only the Condition's one answer counts. */
		bool bHasSources = true;
		bool bNoSourceMet = false;
	};

	/** True if the Rule is met. Never while no actor is potential (with sources). */
	bool IsRuleMet(const FRuleInput& In);

	/** How the actual set changes (spec section 7): indices into the old actual set and the potential set. */
	struct FActualDiff
	{
		/** Old actual entries that stay, in their order. */
		TArray<int32> Kept;
		/** Old actual entries that leave, in their order. */
		TArray<int32> Left;
		/** Potential entries that join, in potential order. */
		TArray<int32> Joined;
	};

	/**
	 * OldActual: each old actual actor (null if it's gone). Potential: each potential actor, with its Condition answer
	 * in Satisfied (same length). An old one stays if it's still potential and satisfied; any other old one leaves.
	 * A satisfied potential one joins if it isn't staying. The new actual set is Kept, then Joined.
	 */
	FActualDiff DiffActual(TConstArrayView<const void*> OldActual, TConstArrayView<const void*> Potential, TConstArrayView<bool> Satisfied);

	/** What the trigger does about its Rule (spec sections 7, 9). */
	enum class EStep : uint8
	{
		None,
		/** Idle and met: start the Activation Timer, or activate. */
		StartActivation,
		/** Activating, not met, with a Timeout timer: back to Idle. */
		CancelActivation,
		/** Active and not met: start the Deactivation Timer, or deactivate. */
		StartDeactivation,
		/** Deactivating, met, with a Timeout timer: back to Active. */
		CancelDeactivation,
		/** Undetermined and met: activate at once (no timer), as the initial state. */
		SettleActivated,
		/** Undetermined and not met: deactivate at once (no timer), as the initial state. */
		SettleDeactivated,
	};

	EStep NextStep(EBangoTriggerState State, bool bRuleMet, EBangoTriggerTimerMode ActivationMode, EBangoTriggerTimerMode DeactivationMode);

	/** True if, deactivating after ActivationCount activations, the trigger is finished. */
	bool IsFinishedAfterDeactivation(int32 MaxActivations, int32 ActivationCount);
}
