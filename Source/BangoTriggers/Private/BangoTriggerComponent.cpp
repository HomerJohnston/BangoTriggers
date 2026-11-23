#include "BangoTriggerComponent.h"

#include "Bango.h"
#include "BangoActorRegistry.h"
#include "BangoIDActorComponent.h"
#include "BangoInputsSignature.h"
#include "BangoLog.h"
#include "BangoScriptSubsystem.h"
#include "BangoTriggerLogic.h"
#include "BangoTriggerShapeComponent.h"
#include "BangoTriggerSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace
{
	const TCHAR* const TriggerInput = TEXT("_trigger");
	const TCHAR* const ActivatorInput = TEXT("_activator");
	const TCHAR* const ActualInput = TEXT("_actual");
	const TCHAR* const PotentialInput = TEXT("_potential");

	const FName ActivatorSignature(TEXT("Activator"));
	const FName ActivatedSignature(TEXT("Activated"));
	const FName DeactivatedSignature(TEXT("Deactivated"));
	const FName ConditionSignature(TEXT("Condition"));

	FBangoValue NilEntity()
	{
		return FBangoValue::MakeNil(FBangoType::Entity());
	}
}

// ---- Types ----

float FBangoTriggerTimer::PickTime() const
{
	const float A = FMath::Max(0.0f, MinTime);
	const float B = FMath::Max(0.0f, MaxTime);
	return A == B ? A : FMath::FRandRange(FMath::Min(A, B), FMath::Max(A, B));
}

// ---- Construction ----

UBangoTriggerComponent::UBangoTriggerComponent()
{
	// UBangoTriggerSubsystem ticks every trigger in one place.
	PrimaryComponentTick.bCanEverTick = false;

	// The inputs the trigger gives its scripts (MakeEventArgs passes them in this order) and its Condition (by name).
	if (IsTemplate())
	{
		const TArray<FBangoInputDecl> EventInputs = {
			{ TEXT("entity _trigger"),      TEXT("This trigger (the component). Its actor is self.") },
			{ TEXT("entity[] _activators"), TEXT("The actors this event is about. A destroyed one is nil.") },
			{ TEXT("entity[] _actual"),     TEXT("The actors that count: potential, and the Condition is true for them, in the order they joined.") },
			{ TEXT("entity[] _potential"),  TEXT("The gathered actors in the right place (inside, or outside with Not Present).") },
		};
		Bango::RegisterInputsSignature(this, ActivatorSignature, EventInputs);

		const TCHAR* const InitialDescription =
			TEXT("True when the trigger (its Start State Undetermined) is settling into its first state, as play begins or after a reset: snap to it rather than animate.");

		TArray<FBangoInputDecl> ActivatedInputs = EventInputs;
		ActivatedInputs.Emplace(TEXT("int _deactivatedScriptHandle"),
			TEXT("The id of the On Deactivated script while it's still running (for Abort, IsRunning or WaitFor), else 0."));
		ActivatedInputs.Emplace(TEXT("bool _initial"), InitialDescription);
		Bango::RegisterInputsSignature(this, ActivatedSignature, ActivatedInputs);

		TArray<FBangoInputDecl> DeactivatedInputs = EventInputs;
		DeactivatedInputs.Emplace(TEXT("int _activatedScriptHandle"),
			TEXT("The id of the On Activated script while it's still running (for Abort, IsRunning or WaitFor), else 0."));
		DeactivatedInputs.Emplace(TEXT("bool _initial"), InitialDescription);
		Bango::RegisterInputsSignature(this, DeactivatedSignature, DeactivatedInputs);

		Bango::RegisterInputsSignature(this, ConditionSignature, {
			{ TEXT("entity _trigger"),     TEXT("This trigger (the component). Its actor is self.") },
			{ TEXT("entity _activator"),   TEXT("The actor being checked; nil if the trigger has no sources.") },
			{ TEXT("entity[] _actual"),    TEXT("The actors that count, as the set last changed.") },
			{ TEXT("entity[] _potential"), TEXT("The gathered actors in the right place, from the last check.") },
		});
	}
	Condition.UseInputsSignature(this, ConditionSignature);
	ActivatorInScript.UseInputsSignature(this, ActivatorSignature);
	ActivatorOutScript.UseInputsSignature(this, ActivatorSignature);
	ActivatedScript.UseInputsSignature(this, ActivatedSignature);
	DeactivatedScript.UseInputsSignature(this, DeactivatedSignature);
}

// ---- Anchor and editor drawing ----

FTransform UBangoTriggerComponent::GetAnchorTransform() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->GetActorTransform() : FTransform::Identity;
}

FString UBangoTriggerComponent::GetTriggerName() const
{
	const AActor* Owner = GetOwner();
	return Owner ? FString::Printf(TEXT("%s.%s"), *Owner->GetActorNameOrLabel(), *GetName()) : GetName();
}

void UBangoTriggerComponent::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITORONLY_DATA
	// As scene components make their editor sprite: only in editor worlds (the level, a Blueprint's viewport), never in
	// game, so a trigger costs nothing to move while playing. On the root, so it follows the actor in the editor.
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	// IsValid: after undoing this component's removal, the pointer (transient, so not undone) names the destroyed one.
	if (!IsValid(ShapeComponent) && Owner && World && !World->IsGameWorld())
	{
		const EObjectFlags TransactionalFlag = GetFlags() & RF_Transactional;
		ShapeComponent = NewObject<UBangoTriggerShapeComponent>(Owner, NAME_None, TransactionalFlag | RF_Transient | RF_TextExportTransient);
		ShapeComponent->SetTrigger(this);
		ShapeComponent->SetIsVisualizationComponent(true);
		ShapeComponent->CreationMethod = CreationMethod;
		if (USceneComponent* Root = Owner->GetRootComponent())
		{
			ShapeComponent->SetupAttachment(Root);
		}
		ShapeComponent->RegisterComponent();
	}
#endif
	RegionsChanged();
}

void UBangoTriggerComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
#if WITH_EDITORONLY_DATA
	if (IsValid(ShapeComponent))
	{
		ShapeComponent->DestroyComponent();
		ShapeComponent = nullptr;
	}
#endif
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UBangoTriggerComponent::RegionsChanged()
{
#if WITH_EDITORONLY_DATA
	if (IsValid(ShapeComponent))
	{
		ShapeComponent->ShapeChanged();
	}
#endif
}

// ---- Constants ----

bool UBangoTriggerComponent::UpdateConstants(FString* OutError)
{
	TArray<FBangoInputDecl> Inputs;
	for (const FBangoTriggerConstant& Constant : Constants)
	{
		Inputs.Emplace(Constant.GetDeclaration());
	}

	// The same list for each: setting it again when nothing changed does nothing. A wrong one is shown as the
	// scripts' error (in the editor too), so only the first is passed on.
	bool bAllGood = true;
	FString FirstError;
	for (FBangoInlineScriptBase* Script : TArray<FBangoInlineScriptBase*>{ &ActivatorInScript, &ActivatorOutScript, &ActivatedScript, &DeactivatedScript, &Condition })
	{
		FString Error;
		if (!Script->SetInstanceInputs(Inputs, &Error))
		{
			if (bAllGood)
			{
				FirstError = Error;
			}
			bAllGood = false;
		}
	}
	if (OutError)
	{
		*OutError = FirstError;
	}
	return bAllGood;
}

void UBangoTriggerComponent::PostInitProperties()
{
	Super::PostInitProperties();
	// Made from a template (a Blueprint's, a placed actor's copy): its Constants are already copied in.
	UpdateConstants();
}

void UBangoTriggerComponent::PostLoad()
{
	Super::PostLoad();
	UpdateConstants();
}

void UBangoTriggerComponent::PostApplyToComponent()
{
	Super::PostApplyToComponent();
	// A Blueprint's trigger is made again whenever its actor's construction script reruns, and its edited properties
	// (Constants among them) are put back afterwards, with no PostLoad or PostEditChangeProperty. BangoScripts keeps
	// the scripts' instance inputs as a property too (v0.86), so they're restored as well, but this keeps the two in
	// step by design.
	UpdateConstants();
}

#if WITH_EDITOR
void UBangoTriggerComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	UpdateConstants();
	RegionsChanged();
}

void UBangoTriggerComponent::PostEditUndo()
{
	Super::PostEditUndo();
	UpdateConstants();
	RegionsChanged();
}

void UBangoTriggerComponent::PostEditImport()
{
	Super::PostEditImport();
	UpdateConstants();
}
#endif

bool UBangoTriggerComponent::IsInsideRegions(const FVector& Location) const
{
	const FTransform Transform = GetAnchorTransform();
	for (const TInstancedStruct<FBangoTriggerRegion>& Region : Regions)
	{
		if (const FBangoTriggerRegion* Ptr = Region.GetPtr())
		{
			if (Ptr->IsInside(Transform, Location))
			{
				return true;
			}
		}
	}
	return false;
}

// ---- Play ----

bool UBangoTriggerComponent::ShouldRunHere() const
{
	const bool bIsClient = GetNetMode() == NM_Client;
	switch (RunsOn)
	{
	case EBangoTriggerRunsOn::Server:   return !bIsClient;
	case EBangoTriggerRunsOn::Client:   return bIsClient;
	case EBangoTriggerRunsOn::Anywhere: return true;
	}
	return false;
}

void UBangoTriggerComponent::BeginPlay()
{
	Super::BeginPlay();

	bEnabled = bStartEnabled;
	State = GetStartingState();
	ActivationCount = 0;
	bRunning = ShouldRunHere();
	if (!bRunning)
	{
		return;
	}
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		bRunning = false;
		return;
	}
	if (!Regions.IsEmpty() && !Owner->GetRootComponent())
	{
		// No place to put the regions: the trigger would quietly use the world's origin.
		Bango::ReportError(FString::Printf(TEXT("The trigger %s has regions, but its actor has no root component to place them from, so it doesn't run."), *GetTriggerName()));
		bRunning = false;
		return;
	}

	// This component, as `_trigger`: its entity name is made from its actor's. The actor's Bango ID component may not
	// have begun play yet (components begin in any order), so its name is registered first; registering again is a no-op.
	if (UBangoIDActorComponent* ID = Owner->FindComponentByClass<UBangoIDActorComponent>())
	{
		ID->RegisterName();
	}
	FString Error;
	if (!Bango::ObjectToEntity(this, TriggerEntity, Error))
	{
		Bango::ReportError(FString::Printf(TEXT("The trigger %s can't be _trigger in its scripts: %s"), *GetTriggerName(), *Error));
		TriggerEntity = NilEntity();
	}

	// Their own inputs, then their values. A wrong list shows as the scripts' compile errors, next; a wrong value is
	// reported here, and is nil. (An actor's Bango ID may not have begun play yet: GetValue registers it first.)
	UpdateConstants();
	ConstantValues.Reset();
	ConstantInputs.Reset();
	for (const FBangoTriggerConstant& Constant : Constants)
	{
		FBangoValue Value;
		FString ValueError;
		if (!Constant.GetValue(Value, ValueError))
		{
			Bango::ReportError(FString::Printf(TEXT("The trigger %s's constant %s can't be used: %s"), *GetTriggerName(), *Constant.Name, *ValueError));
		}
		ConstantValues.Add(Value);
		ConstantInputs.Add({ Constant.Name, Value });
	}

	// Owned by the actor, which is `self` in them.
	ActivatorInCode.Prepare(ActivatorInScript, Owner);
	ActivatorOutCode.Prepare(ActivatorOutScript, Owner);
	ActivatedCode.Prepare(ActivatedScript, Owner);
	DeactivatedCode.Prepare(DeactivatedScript, Owner);
	SetUpCondition();

	if (UBangoTriggerSubsystem* Triggers = UBangoTriggerSubsystem::Get(this))
	{
		// Spread the checks of triggers with the same interval across frames; but an Undetermined one settles on the
		// first frame, so things start in their right state.
		const double Now = GetWorld()->GetTimeSeconds();
		NextCheckTime = State == EBangoTriggerState::Undetermined ? Now : Now + FMath::FRandRange(0.0, (double)PollInterval);
		Triggers->RegisterTrigger(this);
	}
}

void UBangoTriggerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UBangoTriggerSubsystem* Triggers = UBangoTriggerSubsystem::Get(this))
	{
		Triggers->UnregisterTrigger(this);
	}
	TearDownCondition();
	bRunning = false;
	Super::EndPlay(EndPlayReason);
}

// ---- Condition ----

void UBangoTriggerComponent::SetUpCondition()
{
	bConditionBroken = false;
	bNoSourceMet = false;
	if (IsManualTrigger())
	{
		// Nothing to watch.
		return;
	}
	UBangoScriptSubsystem* Scripts = UBangoScriptSubsystem::Get(this);

	if (!Condition.IsEmpty() && Scripts)
	{
		TWeakObjectPtr<UBangoTriggerComponent> WeakThis(this);
		FString Error;
		WatchGroup = Scripts->CreateWatchGroup(Condition, FString::Printf(TEXT("%s.Condition"), *GetTriggerName()),
			[WeakThis](TConstArrayView<FBangoWatchChange> Changes)
			{
				if (UBangoTriggerComponent* Trigger = WeakThis.Get())
				{
					Trigger->OnWatchChanges(Changes);
				}
			}, Error);
		if (!WatchGroup.IsValid())
		{
			Bango::ReportError(Error);
			bConditionBroken = true;
		}
		else if (!ConstantInputs.IsEmpty() && !Scripts->SetGroupInputs(WatchGroup, ConstantInputs, Error))
		{
			// The constants never change: set once, for every watch.
			Bango::ReportError(Error);
		}
	}
	else if (!Condition.IsEmpty())
	{
		bConditionBroken = true;
	}
	UpdateGroupInputs();

	if (Sources.IsEmpty())
	{
		if (WatchGroup.IsValid())
		{
			bNoSourceMet = Scripts->StartWatch(WatchGroup, { { ActivatorInput, NilEntity() } }, NoSourceWatch);
		}
		else
		{
			bNoSourceMet = !bConditionBroken;
		}
	}
}

void UBangoTriggerComponent::TearDownCondition()
{
	if (UBangoScriptSubsystem* Scripts = UBangoScriptSubsystem::Get(this))
	{
		// Destroying the group stops all its watches.
		Scripts->DestroyWatchGroup(WatchGroup);
	}
	WatchGroup = FBangoWatchGroupHandle();
	NoSourceWatch = FBangoWatchHandle();
	for (FPotentialActor& Entry : Potential)
	{
		Entry.Watch = FBangoWatchHandle();
	}
}

bool UBangoTriggerComponent::StartWatchFor(FPotentialActor& Entry)
{
	if (WatchGroup.IsValid())
	{
		if (UBangoScriptSubsystem* Scripts = UBangoScriptSubsystem::Get(this))
		{
			return Scripts->StartWatch(WatchGroup, { { ActivatorInput, Entry.Entity } }, Entry.Watch);
		}
		return false;
	}
	// No Condition: always true. One that didn't compile: never.
	return !bConditionBroken;
}

void UBangoTriggerComponent::OnWatchChanges(TConstArrayView<FBangoWatchChange> Changes)
{
	if (!bRunning || State == EBangoTriggerState::Finished)
	{
		return;
	}
	for (const FBangoWatchChange& Change : Changes)
	{
		if (Change.Watch == NoSourceWatch)
		{
			bNoSourceMet = Change.bSatisfied;
			continue;
		}
		for (FPotentialActor& Entry : Potential)
		{
			if (Entry.Watch == Change.Watch)
			{
				Entry.bSatisfied = Change.bSatisfied;
				break;
			}
		}
	}

	// While disabled, the answers are kept; the next check after enabling catches up.
	if (!bEnabled)
	{
		return;
	}
	if (Sources.IsEmpty())
	{
		UpdateRule({});
	}
	else
	{
		Reconcile(false);
	}
}

void UBangoTriggerComponent::UpdateGroupInputs()
{
	if (!WatchGroup.IsValid())
	{
		return;
	}
	UBangoScriptSubsystem* Scripts = UBangoScriptSubsystem::Get(this);
	if (!Scripts)
	{
		return;
	}

	TArray<FBangoValue> PotentialEntities;
	for (const FPotentialActor& Entry : Potential)
	{
		PotentialEntities.Add(Entry.Entity);
	}
	TArray<FBangoValue> ActualEntities;
	for (const FActualActor& Entry : Actual)
	{
		ActualEntities.Add(Entry.Entity);
	}

	// Setting the same values again does nothing, so this is cheap when nothing changed.
	FString Error;
	Scripts->SetGroupInputs(WatchGroup, {
		{ TriggerInput, TriggerEntity },
		{ PotentialInput, FBangoValue::MakeArray(FBangoType::Entity(), PotentialEntities) },
		{ ActualInput, FBangoValue::MakeArray(FBangoType::Entity(), ActualEntities) },
	}, Error);
}

// ---- Checking ----

void UBangoTriggerComponent::TickTrigger(double Now)
{
	if (!bRunning || !bEnabled || State == EBangoTriggerState::Finished)
	{
		return;
	}

	if (IsManualTrigger())
	{
		// Nothing to check or watch; it only settles, once, if it starts Undetermined.
		if (State == EBangoTriggerState::Undetermined)
		{
			UpdateRule({});
		}
		return;
	}

	if (Now >= NextCheckTime)
	{
		// Keep to the interval's rhythm, but never try to catch up on missed checks.
		NextCheckTime = PollInterval > 0.0f ? FMath::Max(NextCheckTime + PollInterval, Now) : Now;
		Check();
	}

	if ((State == EBangoTriggerState::Activating || State == EBangoTriggerState::Deactivating) && Now >= TimerEnd && IsStillRunning() && bEnabled)
	{
		FinishTimer();
	}
}

void UBangoTriggerComponent::Check()
{
	if (Sources.IsEmpty())
	{
		UpdateRule({});
		return;
	}

	// Who: every source's actors, each once, in source order.
	TArray<AActor*> Found;
	for (const TInstancedStruct<FBangoActivatorSource>& Source : Sources)
	{
		if (const FBangoActivatorSource* Ptr = Source.GetPtr())
		{
			Ptr->Gather(*this, Found);
		}
	}
	// Never the trigger's own actor: a door doesn't open for itself.
	const AActor* Owner = GetOwner();
	TArray<AActor*> NewGathered;
	TSet<const AActor*> Seen;
	for (AActor* Actor : Found)
	{
		if (IsValid(Actor) && Actor != Owner && !Actor->IsActorBeingDestroyed() && !Seen.Contains(Actor))
		{
			Seen.Add(Actor);
			NewGathered.Add(Actor);
		}
	}

	// Where: the potential set, in gathered order. Actors staying potential keep their watch and answer.
	const bool bHasRegions = !Regions.IsEmpty();
	TArray<FPotentialActor> NewPotential;
	bool bPotentialChanged = false;
	for (AActor* Actor : NewGathered)
	{
		const bool bInside = bHasRegions && IsInsideRegions(Actor->GetActorLocation());
		const bool bIsPotential = !bHasRegions || (Presence == EBangoTriggerPresence::Present ? bInside : !bInside);
		if (!bIsPotential)
		{
			continue;
		}
		const int32 Existing = Potential.IndexOfByPredicate([Actor](const FPotentialActor& Entry) { return Entry.Actor.Get() == Actor; });
		if (Existing != INDEX_NONE)
		{
			NewPotential.Add(MoveTemp(Potential[Existing]));
			Potential.RemoveAtSwap(Existing);
		}
		else
		{
			FPotentialActor& Entry = NewPotential.AddDefaulted_GetRef();
			Entry.Actor = Actor;
			Entry.Entity = EntityFor(Actor);
			Entry.bSatisfied = StartWatchFor(Entry);
			bPotentialChanged = true;
		}
	}

	// Those no longer potential (or gone): stop their watches.
	if (!Potential.IsEmpty())
	{
		bPotentialChanged = true;
		if (UBangoScriptSubsystem* Scripts = UBangoScriptSubsystem::Get(this))
		{
			for (FPotentialActor& Entry : Potential)
			{
				Scripts->StopWatch(Entry.Watch);
			}
		}
	}

	Gathered.Reset(NewGathered.Num());
	for (AActor* Actor : NewGathered)
	{
		Gathered.Add(Actor);
	}
	Potential = MoveTemp(NewPotential);

	Reconcile(bPotentialChanged);
}

void UBangoTriggerComponent::Reconcile(bool bPotentialChanged)
{
	TArray<const void*> OldKeys;
	for (const FActualActor& Entry : Actual)
	{
		OldKeys.Add(Entry.Actor.Get());
	}
	TArray<const void*> PotentialKeys;
	TArray<bool> Satisfied;
	for (const FPotentialActor& Entry : Potential)
	{
		PotentialKeys.Add(Entry.Actor.Get());
		Satisfied.Add(Entry.bSatisfied);
	}
	const BangoTriggerLogic::FActualDiff Diff = BangoTriggerLogic::DiffActual(OldKeys, PotentialKeys, Satisfied);

	// The new actual set: those staying, in the order they joined, then those joining, in potential order.
	TArray<FActualActor> NewActual;
	TArray<FChangedActor> Left;
	TArray<FChangedActor> Joined;
	for (const int32 Index : Diff.Kept)
	{
		NewActual.Add(Actual[Index]);
	}
	for (const int32 Index : Diff.Left)
	{
		// A destroyed actor leaves as nil.
		const FActualActor& Entry = Actual[Index];
		Left.Add({ Entry.Actor, Entry.Actor.IsValid() ? Entry.Entity : NilEntity() });
	}
	for (const int32 Index : Diff.Joined)
	{
		const FPotentialActor& Entry = Potential[Index];
		NewActual.Add({ Entry.Actor, Entry.Entity });
		Joined.Add({ Entry.Actor, Entry.Entity });
	}
	Actual = MoveTemp(NewActual);

	if (bPotentialChanged || !Left.IsEmpty() || !Joined.IsEmpty())
	{
		UpdateGroupInputs();
	}

	RunActivatorEvents(Left, Joined);
	if (!IsStillRunning())
	{
		return;
	}

	TArray<FChangedActor> Changed = MoveTemp(Left);
	Changed.Append(Joined);
	UpdateRule(Changed);
}

void UBangoTriggerComponent::RunActivatorEvents(const TArray<FChangedActor>& Left, const TArray<FChangedActor>& Joined)
{
	if (!Left.IsEmpty())
	{
		RunActivatorEvent(ActivatorOutCode, OnActivatorOut, Left);
		if (!IsStillRunning())
		{
			return;
		}
	}
	if (!Joined.IsEmpty())
	{
		RunActivatorEvent(ActivatorInCode, OnActivatorIn, Joined);
	}
}

// ---- Rule and state ----

bool UBangoTriggerComponent::IsRuleMet() const
{
	if (IsManualTrigger())
	{
		return false;
	}
	BangoTriggerLogic::FRuleInput In;
	In.Rule = Rule;
	In.Count = RuleCount;
	In.Min = RuleMin;
	In.Max = RuleMax;
	In.NumGathered = Gathered.Num();
	In.NumPotential = Potential.Num();
	In.NumActual = Actual.Num();
	In.bHasSources = !Sources.IsEmpty();
	In.bNoSourceMet = bNoSourceMet;
	return BangoTriggerLogic::IsRuleMet(In);
}

void UBangoTriggerComponent::UpdateRule(const TArray<FChangedActor>& Changed)
{
	// A manual trigger's state is only ever forced; but an Undetermined one still settles (as Deactivated).
	if (IsManualTrigger() && State != EBangoTriggerState::Undetermined)
	{
		return;
	}
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	switch (BangoTriggerLogic::NextStep(State, IsRuleMet(), ActivationTimer.Mode, DeactivationTimer.Mode))
	{
	case BangoTriggerLogic::EStep::StartActivation:
		PendingChanged = Changed;
		StartActivation(Now);
		break;
	case BangoTriggerLogic::EStep::CancelActivation:
		State = EBangoTriggerState::Idle;
		break;
	case BangoTriggerLogic::EStep::StartDeactivation:
		PendingChanged = Changed;
		StartDeactivation(Now);
		break;
	case BangoTriggerLogic::EStep::CancelDeactivation:
		State = EBangoTriggerState::Active;
		break;
	case BangoTriggerLogic::EStep::SettleActivated:
		// The actors there from the start (they've just run On Activator In) are what it's about.
		PendingChanged = Changed;
		ActivateTrigger(true);
		break;
	case BangoTriggerLogic::EStep::SettleDeactivated:
		// Not about anyone: it's just not active.
		PendingChanged.Reset();
		DeactivateTrigger(true);
		break;
	case BangoTriggerLogic::EStep::None:
		break;
	}
}

void UBangoTriggerComponent::StartActivation(double Now)
{
	if (ActivationTimer.IsUsed())
	{
		State = EBangoTriggerState::Activating;
		TimerEnd = Now + ActivationTimer.PickTime();
	}
	else
	{
		ActivateTrigger(false);
	}
}

void UBangoTriggerComponent::StartDeactivation(double Now)
{
	if (DeactivationTimer.IsUsed())
	{
		State = EBangoTriggerState::Deactivating;
		TimerEnd = Now + DeactivationTimer.PickTime();
	}
	else
	{
		DeactivateTrigger(false);
	}
}

void UBangoTriggerComponent::FinishTimer()
{
	if (State == EBangoTriggerState::Activating)
	{
		// A Timeout timer is cancelled as soon as the Rule stops being met, so reaching the end means it activates.
		ActivateTrigger(false);
	}
	else if (State == EBangoTriggerState::Deactivating)
	{
		DeactivateTrigger(false);
	}
}

void UBangoTriggerComponent::ActivateTrigger(bool bInitial)
{
	++ActivationCount;
	// The state is set before anything runs, since a script or event can change the trigger.
	State = EBangoTriggerState::Active;
	const TArray<FChangedActor> Changed = MoveTemp(PendingChanged);
	PendingChanged.Reset();
	// With the other event's script's id while it's running, so this one can stop it (spec section 8).
	TArray<FBangoValue> Args = MakeEventArgs(Changed);
	Args.Add(FBangoValue::MakeInt(RunningScriptId(DeactivatedScriptHandle)));
	Args.Add(FBangoValue::MakeBool(bInitial));
	Args.Append(ConstantValues);
	ActivatedScriptHandle = SpawnScript(ActivatedCode, Args);
	if (CanBroadcast())
	{
		OnActivated.Broadcast(this, ToActors(Changed), bInitial);
	}

	// A Countdown timer may have activated it after the Rule stopped being met.
	if (IsStillRunning() && State == EBangoTriggerState::Active)
	{
		UpdateRule({});
	}
}

void UBangoTriggerComponent::DeactivateTrigger(bool bInitial)
{
	const bool bFinished = BangoTriggerLogic::IsFinishedAfterDeactivation(MaxActivations, ActivationCount);
	State = bFinished ? EBangoTriggerState::Finished : EBangoTriggerState::Idle;
	const TArray<FChangedActor> Changed = MoveTemp(PendingChanged);
	PendingChanged.Reset();
	TArray<FBangoValue> Args = MakeEventArgs(Changed);
	Args.Add(FBangoValue::MakeInt(RunningScriptId(ActivatedScriptHandle)));
	Args.Add(FBangoValue::MakeBool(bInitial));
	Args.Append(ConstantValues);
	DeactivatedScriptHandle = SpawnScript(DeactivatedCode, Args);
	if (CanBroadcast())
	{
		OnDeactivated.Broadcast(this, ToActors(Changed), bInitial);
	}

	if (bFinished)
	{
		TearDownCondition();
		return;
	}
	// A Countdown timer may have deactivated it while the Rule is met again.
	if (IsStillRunning() && State == EBangoTriggerState::Idle)
	{
		UpdateRule({});
	}
}

double UBangoTriggerComponent::GetTimerTimeLeft() const
{
	if (State != EBangoTriggerState::Activating && State != EBangoTriggerState::Deactivating)
	{
		return -1.0;
	}
	// Paused while disabled.
	if (PausedTimerLeft >= 0.0)
	{
		return PausedTimerLeft;
	}
	const UWorld* World = GetWorld();
	return World ? FMath::Max(0.0, TimerEnd - World->GetTimeSeconds()) : -1.0;
}

EBangoTriggerState UBangoTriggerComponent::GetStartingState() const
{
	return StartState == EBangoTriggerStartState::Undetermined ? EBangoTriggerState::Undetermined : EBangoTriggerState::Idle;
}

bool UBangoTriggerComponent::IsStillRunning() const
{
	// A script or event may have destroyed the component, or its actor.
	return IsValid(this) && !IsBeingDestroyed() && bRunning && State != EBangoTriggerState::Finished;
}

// ---- Events ----

FBangoValue UBangoTriggerComponent::EntityFor(const AActor* Actor)
{
	FBangoValue Value;
	FString Error;
	// Any actor in a game world is an entity (one without a Bango ID gets a Level or Automatic ID); nil only if it fails.
	return Bango::ActorToEntity(Actor, Value, Error) ? Value : NilEntity();
}

TArray<FBangoValue> UBangoTriggerComponent::MakeEventArgs(const TArray<FChangedActor>& Changed) const
{
	TArray<FBangoValue> Activators;
	for (const FChangedActor& Entry : Changed)
	{
		Activators.Add(Entry.Entity);
	}
	TArray<FBangoValue> ActualEntities;
	for (const FActualActor& Entry : Actual)
	{
		ActualEntities.Add(Entry.Entity);
	}
	TArray<FBangoValue> PotentialEntities;
	for (const FPotentialActor& Entry : Potential)
	{
		PotentialEntities.Add(Entry.Entity);
	}
	return {
		TriggerEntity,
		FBangoValue::MakeArray(FBangoType::Entity(), Activators),
		FBangoValue::MakeArray(FBangoType::Entity(), ActualEntities),
		FBangoValue::MakeArray(FBangoType::Entity(), PotentialEntities),
	};
}

int32 UBangoTriggerComponent::RunningScriptId(FBangoScriptHandle Handle) const
{
	const UBangoScriptSubsystem* Scripts = UBangoScriptSubsystem::Get(this);
	// Never nil: Abort(nil) would stop the script calling it.
	return (Scripts && Scripts->IsRunning(Handle)) ? Handle.Id : 0;
}

FBangoScriptHandle UBangoTriggerComponent::SpawnScript(const FBangoTriggerScript& Script, const TArray<FBangoValue>& Args) const
{
	UBangoScriptSubsystem* Scripts = UBangoScriptSubsystem::Get(this);
	return Scripts ? Script.Spawn(*Scripts, Args) : FBangoScriptHandle();
}

void UBangoTriggerComponent::RunActivatorEvent(const FBangoTriggerScript& Script, FBangoTriggerEvent& Event, const TArray<FChangedActor>& Changed)
{
	TArray<FBangoValue> Args = MakeEventArgs(Changed);
	Args.Append(ConstantValues);
	SpawnScript(Script, Args);
	if (CanBroadcast())
	{
		Event.Broadcast(this, ToActors(Changed));
	}
}

bool UBangoTriggerComponent::CanBroadcast() const
{
	return IsValid(this) && !IsBeingDestroyed();
}

TArray<AActor*> UBangoTriggerComponent::ToActors(const TArray<FChangedActor>& Changed)
{
	TArray<AActor*> Actors;
	for (const FChangedActor& Entry : Changed)
	{
		Actors.Add(Entry.Actor.Get());
	}
	return Actors;
}

// ---- Blueprint API ----

void UBangoTriggerComponent::SetTriggerEnabled(bool bEnable)
{
	if (bEnable == bEnabled)
	{
		return;
	}
	bEnabled = bEnable;

	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	const bool bTimerRunning = State == EBangoTriggerState::Activating || State == EBangoTriggerState::Deactivating;
	if (!bEnable)
	{
		PausedTimerLeft = bTimerRunning ? FMath::Max(0.0, TimerEnd - Now) : -1.0;
	}
	else
	{
		if (bTimerRunning && PausedTimerLeft >= 0.0)
		{
			TimerEnd = Now + PausedTimerLeft;
		}
		PausedTimerLeft = -1.0;
		// Check at once, to catch up with what changed while disabled.
		NextCheckTime = Now;
	}
}

TArray<AActor*> UBangoTriggerComponent::GetActualActivators() const
{
	TArray<AActor*> Actors;
	for (const FActualActor& Entry : Actual)
	{
		if (AActor* Actor = Entry.Actor.Get())
		{
			Actors.Add(Actor);
		}
	}
	return Actors;
}

TArray<AActor*> UBangoTriggerComponent::GetPotentialActivators() const
{
	TArray<AActor*> Actors;
	for (const FPotentialActor& Entry : Potential)
	{
		if (AActor* Actor = Entry.Actor.Get())
		{
			Actors.Add(Actor);
		}
	}
	return Actors;
}

TArray<AActor*> UBangoTriggerComponent::GetGatheredActors() const
{
	TArray<AActor*> Actors;
	for (const TWeakObjectPtr<AActor>& Weak : Gathered)
	{
		if (AActor* Actor = Weak.Get())
		{
			Actors.Add(Actor);
		}
	}
	return Actors;
}

void UBangoTriggerComponent::ForceActivate()
{
	if (bRunning && (State == EBangoTriggerState::Undetermined || State == EBangoTriggerState::Idle || State == EBangoTriggerState::Activating))
	{
		PendingChanged.Reset();
		ActivateTrigger(false);
	}
}

void UBangoTriggerComponent::ForceDeactivate()
{
	if (bRunning && (State == EBangoTriggerState::Undetermined || State == EBangoTriggerState::Active || State == EBangoTriggerState::Deactivating))
	{
		PendingChanged.Reset();
		DeactivateTrigger(false);
	}
}

void UBangoTriggerComponent::ResetTrigger()
{
	if (!bRunning)
	{
		return;
	}
	TearDownCondition();
	Gathered.Reset();
	Potential.Reset();
	Actual.Reset();
	PendingChanged.Reset();
	State = GetStartingState();
	ActivationCount = 0;
	PausedTimerLeft = -1.0;
	SetUpCondition();
	if (const UWorld* World = GetWorld())
	{
		NextCheckTime = World->GetTimeSeconds();
	}
}
