#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StructUtils/InstancedStruct.h"
#include "BangoActivatorSource.h"
#include "BangoConditionWatches.h"
#include "BangoInlineScript.h"
#include "BangoTriggerCode.h"
#include "BangoTriggerRegion.h"
#include "BangoTriggerTypes.h"
#include "BangoTypes.h"
#include "BangoTrigger.generated.h"

class UBillboardComponent;
class UBangoTriggerShapeComponent;
class ABangoTrigger;

/** A trigger event, with the actors it's about (destroyed ones are null). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBangoTriggerEvent, ABangoTrigger*, Trigger, const TArray<AActor*>&, Activators);

/**
 * A Bango trigger (spec in Plugins/BangoTriggers/Docs/BangoTriggersSpec.md).
 *
 * Who: its Sources gather actors. Where: its Regions and Presence make some of them potential. Its Condition, watched
 * for each potential actor, makes some of those actual. Its Rule, on how many are actual, says when it's active.
 * Actors joining and leaving the actual set run On Activator In and Out; the trigger switching runs On Activated and
 * On Deactivated.
 */
UCLASS(meta = (DisplayName = "Bango Trigger"), HideCategories = (Rendering, Replication, Collision, Input, HLOD, Physics, Networking, LOD, Cooking, DataLayers, WorldPartition))
class BANGOTRIGGERS_API ABangoTrigger : public AActor
{
	GENERATED_BODY()

public:
	ABangoTrigger();

	// ---- Settings ----

	/** A disabled trigger doesn't check, and its timers pause, but it keeps its state. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger")
	bool bStartEnabled = true;

	/** Where the trigger runs. Elsewhere it does nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger")
	EBangoTriggerRunsOn RunsOn = EBangoTriggerRunsOn::Server;

	/** Seconds between checks of who's where (sources and regions). 0 checks every frame. The Condition is watched instead. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger", meta = (ClampMin = "0", Units = "s"))
	float PollInterval = 0.5f;

	/** How many times it can activate. 0 means no limit. After the last, it deactivates as usual, then it's finished. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger", meta = (ClampMin = "0"))
	int32 MaxActivations = 0;

	/** Who: where the trigger gets its actors from. No sources: only the Condition decides. */
	UPROPERTY(EditAnywhere, Category = "Trigger|Who", meta = (ExcludeBaseStruct))
	TArray<TInstancedStruct<FBangoActivatorSource>> Sources;

	/** Where: an actor's location inside any of these is inside the trigger. No regions: everyone gathered is potential. */
	UPROPERTY(EditAnywhere, Category = "Trigger|Where", meta = (ExcludeBaseStruct))
	TArray<TInstancedStruct<FBangoTriggerRegion>> Regions;

	/** Whether gathered actors must be inside the regions, or outside them, to be potential. Not used with no regions. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|Where")
	EBangoTriggerPresence Presence = EBangoTriggerPresence::Present;

	/**
	 * Watched for each potential actor (_activator): those it's true for are actual. Empty is always true. It can read
	 * _trigger, _activator, _actual and _potential directly.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|Condition")
	FBangoInlineCondition Condition;

	/** When the trigger is active, from how many actors are actual. Never while no actor is potential. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|Rule")
	EBangoTriggerRule Rule = EBangoTriggerRule::AtLeast;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|Rule", meta = (DisplayName = "Count", ClampMin = "0",
		EditCondition = "Rule == EBangoTriggerRule::AtLeast || Rule == EBangoTriggerRule::AtMost", EditConditionHides))
	int32 RuleCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|Rule", meta = (DisplayName = "Min", ClampMin = "0",
		EditCondition = "Rule == EBangoTriggerRule::Between", EditConditionHides))
	int32 RuleMin = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|Rule", meta = (DisplayName = "Max", ClampMin = "0",
		EditCondition = "Rule == EBangoTriggerRule::Between", EditConditionHides))
	int32 RuleMax = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|Timers")
	FBangoTriggerTimer ActivationTimer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|Timers")
	FBangoTriggerTimer DeactivationTimer;

	/** Run when actors join the actual set (_activators). Can read _trigger, _activators, _actual and _potential. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|Scripts", meta = (DisplayName = "On Activator In"))
	FBangoInlineScript ActivatorInScript;

	/** Run when actors leave the actual set (_activators; destroyed ones are nil). Can read _trigger, _activators, _actual and _potential. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|Scripts", meta = (DisplayName = "On Activator Out"))
	FBangoInlineScript ActivatorOutScript;

	/** Run when the trigger activates. Can read _trigger, _activators, _actual and _potential. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|Scripts", meta = (DisplayName = "On Activated"))
	FBangoInlineScript ActivatedScript;

	/** Run when the trigger deactivates. Can read _trigger, _activators, _actual and _potential. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|Scripts", meta = (DisplayName = "On Deactivated"))
	FBangoInlineScript DeactivatedScript;

	// ---- Events ----

	UPROPERTY(BlueprintAssignable, Category = "Trigger")
	FBangoTriggerEvent OnActivatorIn;

	UPROPERTY(BlueprintAssignable, Category = "Trigger")
	FBangoTriggerEvent OnActivatorOut;

	UPROPERTY(BlueprintAssignable, Category = "Trigger")
	FBangoTriggerEvent OnActivated;

	UPROPERTY(BlueprintAssignable, Category = "Trigger")
	FBangoTriggerEvent OnDeactivated;

	// ---- Blueprint API ----

	UFUNCTION(BlueprintCallable, Category = "Bango|Trigger")
	void SetTriggerEnabled(bool bEnable);

	UFUNCTION(BlueprintPure, Category = "Bango|Trigger")
	bool IsTriggerEnabled() const { return bEnabled; }

	UFUNCTION(BlueprintPure, Category = "Bango|Trigger")
	EBangoTriggerState GetTriggerState() const { return State; }

	/** True while Active or Deactivating. */
	UFUNCTION(BlueprintPure, Category = "Bango|Trigger")
	bool IsTriggerActive() const { return State == EBangoTriggerState::Active || State == EBangoTriggerState::Deactivating; }

	UFUNCTION(BlueprintPure, Category = "Bango|Trigger")
	int32 GetActivationCount() const { return ActivationCount; }

	/** The actual set, in the order they joined. */
	UFUNCTION(BlueprintPure, Category = "Bango|Trigger")
	TArray<AActor*> GetActualActivators() const;

	/** The potential set, from the last check. */
	UFUNCTION(BlueprintPure, Category = "Bango|Trigger")
	TArray<AActor*> GetPotentialActivators() const;

	/** Everyone gathered at the last check. */
	UFUNCTION(BlueprintPure, Category = "Bango|Trigger")
	TArray<AActor*> GetGatheredActors() const;

	/** Activates now (counts as an activation) if it isn't active or finished. */
	UFUNCTION(BlueprintCallable, Category = "Bango|Trigger")
	void ForceActivate();

	/** Deactivates now if it's active. */
	UFUNCTION(BlueprintCallable, Category = "Bango|Trigger")
	void ForceDeactivate();

	/** Back to Idle with no activations and empty sets. Nothing is run. */
	UFUNCTION(BlueprintCallable, Category = "Bango|Trigger")
	void ResetTrigger();

	/** Redraws the regions. Call after changing them from code. */
	UFUNCTION(BlueprintCallable, Category = "Bango|Trigger")
	void ShapeChanged();

	// ---- For the subsystem and the editor ----

	/** Called every frame by UBangoTriggerSubsystem, with the world's game time. */
	void TickTrigger(double Now);

	/** True if a world location is inside any region. */
	bool IsInsideRegions(const FVector& Location) const;

	UBangoTriggerShapeComponent* GetShapeComponent() const { return ShapeComponent; }

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trigger")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Trigger")
	TObjectPtr<UBangoTriggerShapeComponent> ShapeComponent;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> Sprite;
#endif

private:
	struct FPotentialActor
	{
		TWeakObjectPtr<AActor> Actor;
		FBangoValue Entity;
		FBangoWatchHandle Watch;
		bool bSatisfied = false;
	};

	struct FActualActor
	{
		TWeakObjectPtr<AActor> Actor;
		FBangoValue Entity;
	};

	/** An actor an event is about. A destroyed one has no Actor and a nil Entity. */
	struct FChangedActor
	{
		TWeakObjectPtr<AActor> Actor;
		FBangoValue Entity;
	};

	bool ShouldRunHere() const;
	/** Makes the Condition's watch group (and, with no sources, its one watch). */
	void SetUpCondition();
	/** Stops every watch and destroys the group. */
	void TearDownCondition();
	/** Starts a potential actor's watch; returns its first answer. */
	bool StartWatchFor(FPotentialActor& Potential);
	void OnWatchChanges(TConstArrayView<FBangoWatchChange> Changes);

	/** Who's where: gathers, works out the potential set (starting and stopping watches), then Reconcile. */
	void Check();
	/** Brings the actual set up to date with the potential actors' answers, runs In and Out, then tests the Rule. */
	void Reconcile(bool bPotentialChanged);
	void UpdateGroupInputs();
	void RunActivatorEvents(const TArray<FChangedActor>& Left, const TArray<FChangedActor>& Joined);

	bool IsRuleMet() const;
	void UpdateRule(const TArray<FChangedActor>& Changed);
	void StartActivation(double Now);
	void StartDeactivation(double Now);
	void FinishTimer();
	void Activate();
	void Deactivate();
	/** True if the trigger is still running normally after something that may have changed it (a script, an event). */
	bool IsStillRunning() const;

	/** The "Activation" inputs, in order: _trigger, _activators, _actual, _potential. */
	TArray<FBangoValue> MakeActivationArgs(const TArray<FChangedActor>& Changed) const;
	void RunEvent(const FBangoTriggerScript& Script, FBangoTriggerEvent& Event, const TArray<FChangedActor>& Changed);
	static FBangoValue EntityFor(const AActor* Actor);

	bool bRunning = false;
	bool bEnabled = true;
	EBangoTriggerState State = EBangoTriggerState::Idle;
	int32 ActivationCount = 0;

	double NextCheckTime = 0.0;
	double TimerEnd = 0.0;
	double PausedTimerLeft = -1.0;
	/** The change that started the running timer: the activated or deactivated event's _activators. */
	TArray<FChangedActor> PendingChanged;

	TArray<TWeakObjectPtr<AActor>> Gathered;
	TArray<FPotentialActor> Potential;
	TArray<FActualActor> Actual;
	FBangoValue TriggerEntity;

	FBangoWatchGroupHandle WatchGroup;
	/** With no sources: the Condition's one watch, and its answer. */
	FBangoWatchHandle NoSourceWatch;
	bool bNoSourceMet = false;
	/** The Condition didn't compile: it's never true. */
	bool bConditionBroken = false;

	FBangoTriggerScript ActivatorInCode;
	FBangoTriggerScript ActivatorOutCode;
	FBangoTriggerScript ActivatedCode;
	FBangoTriggerScript DeactivatedCode;
};
