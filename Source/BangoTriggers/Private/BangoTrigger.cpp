#include "BangoTrigger.h"

#include "Bango.h"
#include "BangoActorRegistry.h"
#include "BangoInputsSignature.h"
#include "BangoLog.h"
#include "BangoScriptSubsystem.h"
#include "BangoTriggerLogic.h"
#include "BangoTriggerShapeComponent.h"
#include "BangoTriggerSubsystem.h"
#include "Components/BillboardComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const TCHAR* const TriggerInput = TEXT("_trigger");
	const TCHAR* const ActivatorInput = TEXT("_activator");
	const TCHAR* const ActualInput = TEXT("_actual");
	const TCHAR* const PotentialInput = TEXT("_potential");

	const FName ActivationSignature(TEXT("Activation"));
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

ABangoTrigger::ABangoTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	ShapeComponent = CreateDefaultSubobject<UBangoTriggerShapeComponent>(TEXT("Shape"));
	ShapeComponent->SetupAttachment(Root);

#if WITH_EDITORONLY_DATA
	Sprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (Sprite && !IsRunningCommandlet())
	{
		static ConstructorHelpers::FObjectFinderOptional<UTexture2D> TriggerTexture(TEXT("/Engine/EditorResources/S_Trigger"));
		Sprite->Sprite = TriggerTexture.Get();
		Sprite->SetupAttachment(Root);
		Sprite->bIsScreenSizeScaled = true;
		Sprite->SetRelativeScale3D(FVector(0.5f));
	}
#endif

	// A new trigger: "a player enters this area".
	Sources.Add(TInstancedStruct<FBangoActivatorSource>::Make<FBangoPlayersSource>());
	Regions.Add(TInstancedStruct<FBangoTriggerRegion>::Make<FBangoAreaRegion>());

	// The inputs the trigger gives its scripts (MakeActivationArgs passes them in this order) and its Condition (by name).
	if (IsTemplate())
	{
		Bango::RegisterInputsSignature(this, ActivationSignature, {
			{ TEXT("entity _trigger"),      TEXT("The trigger itself, or nil if it has no Bango ID.") },
			{ TEXT("entity[] _activators"), TEXT("The actors this event is about. A destroyed one is nil.") },
			{ TEXT("entity[] _actual"),     TEXT("The actors that count: potential, and the Condition is true for them, in the order they joined.") },
			{ TEXT("entity[] _potential"),  TEXT("The gathered actors in the right place (inside, or outside with Not Present).") },
		});
		Bango::RegisterInputsSignature(this, ConditionSignature, {
			{ TEXT("entity _trigger"),     TEXT("The trigger itself, or nil if it has no Bango ID.") },
			{ TEXT("entity _activator"),   TEXT("The actor being checked; nil if the trigger has no sources.") },
			{ TEXT("entity[] _actual"),    TEXT("The actors that count, as the set last changed.") },
			{ TEXT("entity[] _potential"), TEXT("The gathered actors in the right place, from the last check.") },
		});
	}
	Condition.UseInputsSignature(this, ConditionSignature);
	ActivatorInScript.UseInputsSignature(this, ActivationSignature);
	ActivatorOutScript.UseInputsSignature(this, ActivationSignature);
	ActivatedScript.UseInputsSignature(this, ActivationSignature);
	DeactivatedScript.UseInputsSignature(this, ActivationSignature);
}

void ABangoTrigger::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// Runs after every change in the editor (properties, undo, moving), so the drawing always follows.
	ShapeChanged();
}

void ABangoTrigger::ShapeChanged()
{
	if (ShapeComponent)
	{
		ShapeComponent->ShapeChanged();
	}
}

bool ABangoTrigger::IsInsideRegions(const FVector& Location) const
{
	const FTransform& Transform = GetActorTransform();
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

bool ABangoTrigger::ShouldRunHere() const
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

void ABangoTrigger::BeginPlay()
{
	Super::BeginPlay();

	bEnabled = bStartEnabled;
	State = EBangoTriggerState::Idle;
	ActivationCount = 0;
	bRunning = ShouldRunHere();
	if (!bRunning)
	{
		return;
	}

	// After Super, so the trigger's own Bango ID component (if any) has registered.
	TriggerEntity = EntityFor(this);

	ActivatorInCode.Prepare(ActivatorInScript, this);
	ActivatorOutCode.Prepare(ActivatorOutScript, this);
	ActivatedCode.Prepare(ActivatedScript, this);
	DeactivatedCode.Prepare(DeactivatedScript, this);
	SetUpCondition();

	if (UBangoTriggerSubsystem* Triggers = UBangoTriggerSubsystem::Get(this))
	{
		// Spread the checks of triggers with the same interval across frames.
		NextCheckTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(0.0, (double)PollInterval);
		Triggers->RegisterTrigger(this);
	}
}

void ABangoTrigger::EndPlay(const EEndPlayReason::Type EndPlayReason)
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

void ABangoTrigger::SetUpCondition()
{
	bConditionBroken = false;
	bNoSourceMet = false;
	UBangoScriptSubsystem* Scripts = UBangoScriptSubsystem::Get(this);

	if (!Condition.IsEmpty() && Scripts)
	{
		TWeakObjectPtr<ABangoTrigger> WeakThis(this);
		FString Error;
		WatchGroup = Scripts->CreateWatchGroup(Condition, FString::Printf(TEXT("%s.Condition"), *GetActorNameOrLabel()),
			[WeakThis](TConstArrayView<FBangoWatchChange> Changes)
			{
				if (ABangoTrigger* Trigger = WeakThis.Get())
				{
					Trigger->OnWatchChanges(Changes);
				}
			}, Error);
		if (!WatchGroup.IsValid())
		{
			Bango::ReportError(Error);
			bConditionBroken = true;
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

void ABangoTrigger::TearDownCondition()
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

bool ABangoTrigger::StartWatchFor(FPotentialActor& Entry)
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

void ABangoTrigger::OnWatchChanges(TConstArrayView<FBangoWatchChange> Changes)
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

void ABangoTrigger::UpdateGroupInputs()
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

void ABangoTrigger::TickTrigger(double Now)
{
	if (!bRunning || !bEnabled || State == EBangoTriggerState::Finished)
	{
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

void ABangoTrigger::Check()
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
	TArray<AActor*> NewGathered;
	TSet<const AActor*> Seen;
	for (AActor* Actor : Found)
	{
		if (IsValid(Actor) && Actor != this && !Actor->IsActorBeingDestroyed() && !Seen.Contains(Actor))
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

void ABangoTrigger::Reconcile(bool bPotentialChanged)
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

void ABangoTrigger::RunActivatorEvents(const TArray<FChangedActor>& Left, const TArray<FChangedActor>& Joined)
{
	if (!Left.IsEmpty())
	{
		RunEvent(ActivatorOutCode, OnActivatorOut, Left);
		if (!IsStillRunning())
		{
			return;
		}
	}
	if (!Joined.IsEmpty())
	{
		RunEvent(ActivatorInCode, OnActivatorIn, Joined);
	}
}

// ---- Rule and state ----

bool ABangoTrigger::IsRuleMet() const
{
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

void ABangoTrigger::UpdateRule(const TArray<FChangedActor>& Changed)
{
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
	case BangoTriggerLogic::EStep::None:
		break;
	}
}

void ABangoTrigger::StartActivation(double Now)
{
	if (ActivationTimer.IsUsed())
	{
		State = EBangoTriggerState::Activating;
		TimerEnd = Now + ActivationTimer.PickTime();
	}
	else
	{
		Activate();
	}
}

void ABangoTrigger::StartDeactivation(double Now)
{
	if (DeactivationTimer.IsUsed())
	{
		State = EBangoTriggerState::Deactivating;
		TimerEnd = Now + DeactivationTimer.PickTime();
	}
	else
	{
		Deactivate();
	}
}

void ABangoTrigger::FinishTimer()
{
	if (State == EBangoTriggerState::Activating)
	{
		// A Timeout timer is cancelled as soon as the Rule stops being met, so reaching the end means it activates.
		Activate();
	}
	else if (State == EBangoTriggerState::Deactivating)
	{
		Deactivate();
	}
}

void ABangoTrigger::Activate()
{
	++ActivationCount;
	// The state is set before anything runs, since a script or event can change the trigger.
	State = EBangoTriggerState::Active;
	const TArray<FChangedActor> Changed = MoveTemp(PendingChanged);
	PendingChanged.Reset();
	RunEvent(ActivatedCode, OnActivated, Changed);

	// A Countdown timer may have activated it after the Rule stopped being met.
	if (IsStillRunning() && State == EBangoTriggerState::Active)
	{
		UpdateRule({});
	}
}

void ABangoTrigger::Deactivate()
{
	const bool bFinished = BangoTriggerLogic::IsFinishedAfterDeactivation(MaxActivations, ActivationCount);
	State = bFinished ? EBangoTriggerState::Finished : EBangoTriggerState::Idle;
	const TArray<FChangedActor> Changed = MoveTemp(PendingChanged);
	PendingChanged.Reset();
	RunEvent(DeactivatedCode, OnDeactivated, Changed);

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

bool ABangoTrigger::IsStillRunning() const
{
	return IsValid(this) && bRunning && State != EBangoTriggerState::Finished;
}

// ---- Events ----

FBangoValue ABangoTrigger::EntityFor(const AActor* Actor)
{
	FBangoValue Value;
	FString Error;
	// For now every actor a trigger uses is assumed to have a Bango ID; one without is nil (spec section 8).
	return Bango::ActorToEntity(Actor, Value, Error) ? Value : NilEntity();
}

TArray<FBangoValue> ABangoTrigger::MakeActivationArgs(const TArray<FChangedActor>& Changed) const
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

void ABangoTrigger::RunEvent(const FBangoTriggerScript& Script, FBangoTriggerEvent& Event, const TArray<FChangedActor>& Changed)
{
	if (UBangoScriptSubsystem* Scripts = UBangoScriptSubsystem::Get(this))
	{
		Script.Spawn(*Scripts, MakeActivationArgs(Changed));
	}
	if (!IsValid(this))
	{
		return;
	}
	TArray<AActor*> Actors;
	for (const FChangedActor& Entry : Changed)
	{
		Actors.Add(Entry.Actor.Get());
	}
	Event.Broadcast(this, Actors);
}

// ---- Blueprint API ----

void ABangoTrigger::SetTriggerEnabled(bool bEnable)
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

TArray<AActor*> ABangoTrigger::GetActualActivators() const
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

TArray<AActor*> ABangoTrigger::GetPotentialActivators() const
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

TArray<AActor*> ABangoTrigger::GetGatheredActors() const
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

void ABangoTrigger::ForceActivate()
{
	if (bRunning && (State == EBangoTriggerState::Idle || State == EBangoTriggerState::Activating))
	{
		PendingChanged.Reset();
		Activate();
	}
}

void ABangoTrigger::ForceDeactivate()
{
	if (bRunning && (State == EBangoTriggerState::Active || State == EBangoTriggerState::Deactivating))
	{
		PendingChanged.Reset();
		Deactivate();
	}
}

void ABangoTrigger::ResetTrigger()
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
	State = EBangoTriggerState::Idle;
	ActivationCount = 0;
	PausedTimerLeft = -1.0;
	SetUpCondition();
	if (const UWorld* World = GetWorld())
	{
		NextCheckTime = World->GetTimeSeconds();
	}
}
