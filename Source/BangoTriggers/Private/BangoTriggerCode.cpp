#include "BangoTriggerCode.h"

#include "Bango.h"
#include "BangoLog.h"
#include "BangoProgram.h"

void FBangoTriggerScript::Prepare(const FBangoInlineScript& InScript, const UObject* InOwner)
{
	Owner = InOwner;
	bUsable = false;
	Script = InScript;
	if (Script.IsEmpty())
	{
		return;
	}

	// The same name and script UBangoScriptSubsystem::Spawn uses, so the program compiled here is the one it runs. The
	// owner is passed too, so a compile error names the object (the trigger's actor), not just its class.
	TSharedPtr<const FBangoProgram> Program;
	FString Error;
	if (!FBangoModule::Get().GetInlineProgram(Script, UBangoScriptSubsystem::GetInlineScriptName(InOwner), Program, Error, nullptr, InOwner)
		|| !Program.IsValid())
	{
		Bango::ReportError(Error);
		return;
	}
	bUsable = true;
}

FBangoScriptHandle FBangoTriggerScript::Spawn(UBangoScriptSubsystem& Scripts, const TArray<FBangoValue>& Args) const
{
	return bUsable ? Scripts.Spawn(Script, Args, Owner.Get()) : FBangoScriptHandle();
}
