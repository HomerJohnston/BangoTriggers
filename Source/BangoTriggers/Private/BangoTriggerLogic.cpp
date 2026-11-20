#include "BangoTriggerLogic.h"

namespace BangoTriggerLogic
{
	bool IsRuleMet(const FRuleInput& In)
	{
		if (!In.bHasSources)
		{
			return In.bNoSourceMet;
		}
		if (In.NumPotential <= 0)
		{
			return false;
		}
		switch (In.Rule)
		{
		case EBangoTriggerRule::All:     return In.NumActual == In.NumGathered;
		case EBangoTriggerRule::AtLeast: return In.NumActual >= In.Count;
		case EBangoTriggerRule::AtMost:  return In.NumActual <= In.Count;
		case EBangoTriggerRule::Between: return In.NumActual >= In.Min && In.NumActual <= In.Max;
		}
		return false;
	}

	FActualDiff DiffActual(TConstArrayView<const void*> OldActual, TConstArrayView<const void*> Potential, TConstArrayView<bool> Satisfied)
	{
		check(Potential.Num() == Satisfied.Num());

		auto IsSatisfiedPotential = [&](const void* Key)
		{
			for (int32 Index = 0; Index < Potential.Num(); ++Index)
			{
				if (Potential[Index] == Key)
				{
					return Satisfied[Index];
				}
			}
			return false;
		};

		FActualDiff Diff;
		for (int32 Index = 0; Index < OldActual.Num(); ++Index)
		{
			const void* Key = OldActual[Index];
			if (Key && IsSatisfiedPotential(Key))
			{
				Diff.Kept.Add(Index);
			}
			else
			{
				Diff.Left.Add(Index);
			}
		}
		for (int32 Index = 0; Index < Potential.Num(); ++Index)
		{
			const void* Key = Potential[Index];
			if (!Key || !Satisfied[Index])
			{
				continue;
			}
			const bool bStaying = Diff.Kept.ContainsByPredicate([&](int32 Kept) { return OldActual[Kept] == Key; });
			const bool bAlreadyJoined = Diff.Joined.ContainsByPredicate([&](int32 Joined) { return Potential[Joined] == Key; });
			if (!bStaying && !bAlreadyJoined)
			{
				Diff.Joined.Add(Index);
			}
		}
		return Diff;
	}

	EStep NextStep(EBangoTriggerState State, bool bRuleMet, EBangoTriggerTimerMode ActivationMode, EBangoTriggerTimerMode DeactivationMode)
	{
		switch (State)
		{
		case EBangoTriggerState::Idle:
			return bRuleMet ? EStep::StartActivation : EStep::None;
		case EBangoTriggerState::Activating:
			return (!bRuleMet && ActivationMode == EBangoTriggerTimerMode::Timeout) ? EStep::CancelActivation : EStep::None;
		case EBangoTriggerState::Active:
			return bRuleMet ? EStep::None : EStep::StartDeactivation;
		case EBangoTriggerState::Deactivating:
			return (bRuleMet && DeactivationMode == EBangoTriggerTimerMode::Timeout) ? EStep::CancelDeactivation : EStep::None;
		case EBangoTriggerState::Finished:
			return EStep::None;
		}
		return EStep::None;
	}

	bool IsFinishedAfterDeactivation(int32 MaxActivations, int32 ActivationCount)
	{
		return MaxActivations > 0 && ActivationCount >= MaxActivations;
	}
}
