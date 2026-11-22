#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StructUtils/InstancedStruct.h"
#include "BangoActivatorSource.h"
#include "BangoConditionWatches.h"
#include "BangoInlineScript.h"
#include "BangoTriggerCode.h"
#include "BangoTriggerConstant.h"
#include "BangoTriggerRegion.h"
#include "BangoTriggerTypes.h"
#include "BangoTypes.h"
#include "BangoTriggerComponent.generated.h"

class UBangoTriggerComponent;
class UBangoTriggerShapeComponent;

/** An activator event (In or Out), with the actors it's about (destroyed ones are null). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBangoTriggerEvent, UBangoTriggerComponent*, Trigger, const TArray<AActor*>&, Activators);

/**
 * The trigger activating or deactivating, with the actors it's about (destroyed ones are null). Initial: it's an
 * Undetermined trigger settling into its first state, as play begins (or after a reset).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FBangoTriggerStateEvent, UBangoTriggerComponent*, Trigger, const TArray<AActor*>&, Activators, bool, bInitial);

/**
 * A Bango trigger (spec in Plugins/BangoTriggers/Docs/BangoTriggersSpec.md), on any actor: a door can open itself.
 *
 * Who: its Sources gather actors (never its own actor). Where: its Regions, placed from its actor's transform (its
 * anchor), and Presence make some of them potential. Its Condition, watched for each potential actor, makes some of
 * those actual. Its Rule, on how many are actual, says when it's active. Actors joining and leaving the actual set run
 * On Activator In and Out; the trigger switching runs On Activated and On Deactivated.
 *
 * A plain actor component, not a scene component: moving its actor costs it nothing. It reads the anchor only when it
 * checks. Its scripts belong to its actor (`self`); `_trigger` is this component.
 */
UCLASS(ClassGroup = (Bango), meta = (BlueprintSpawnableComponent, DisplayName = "Bango Trigger"), HideCategories = (Activation, Cooking, Replication, ComponentReplication, Navigation, AssetUserData))
class BANGOTRIGGERS_API UBangoTriggerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBangoTriggerComponent();

	// ---- Settings ----

	/** A disabled trigger doesn't check, and its timers pause, but it keeps its state. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger")
	bool bStartEnabled = true;

	/**
	 * Deactivated: it starts idle, and nothing runs until the Rule is first met. Undetermined: for things whose own
	 * starting state isn't known (a light it switches). At its first check it settles at once, without timers: On
	 * Activated runs if the Rule is met, On Deactivated if not, with _initial true. Reset Trigger goes back to this.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger")
	EBangoTriggerStartState StartState = EBangoTriggerStartState::Deactivated;

	/** Where the trigger runs. Elsewhere it does nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger")
	EBangoTriggerRunsOn RunsOn = EBangoTriggerRunsOn::Server;

	/** Seconds between checks of who's where (sources and regions). 0 checks every frame. The Condition is watched instead. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger", meta = (ClampMin = "0", Units = "s", EditCondition = "HasSources", EditConditionHides))
	float PollInterval = 0.5f;

	/** How many times it can activate. 0 means no limit. After the last, it deactivates as usual, then it's finished. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger", meta = (ClampMin = "0"))
	int32 MaxActivations = 0;

	/** Who: where the trigger gets its actors from. Its own actor is never gathered. No sources: only the Condition decides. */
	UPROPERTY(EditAnywhere, Category = "Trigger|Who", meta = (ExcludeBaseStruct))
	TArray<TInstancedStruct<FBangoActivatorSource>> Sources;

	/**
	 * Where: an actor's location inside any of these is inside the trigger. Each is placed from the trigger's actor
	 * (its location, rotation and scale; see each region). No regions: everyone gathered is potential.
	 * Not used with no sources (hidden then, unless it still has regions, so they can be seen and removed).
	 */
	UPROPERTY(EditAnywhere, Category = "Trigger|Where", meta = (ExcludeBaseStruct, EditCondition = "ShowRegions", EditConditionHides))
	TArray<TInstancedStruct<FBangoTriggerRegion>> Regions;

	/** Whether gathered actors must be inside the regions, or outside them, to be potential. Not used with no sources or no regions. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|Where", meta = (EditCondition = "HasSources", EditConditionHides))
	EBangoTriggerPresence Presence = EBangoTriggerPresence::Present;

	/**
	 * Watched for each potential actor (_activator): those it's true for are actual. Empty is always true. It can read
	 * _trigger, _activator, _actual and _potential directly.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|Condition")
	FBangoInlineCondition Condition;

	/** When the trigger is active, from how many actors are actual. Never while no actor is potential. Not used with no sources. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|Rule", meta = (EditCondition = "HasSources", EditConditionHides))
	EBangoTriggerRule Rule = EBangoTriggerRule::AtLeast;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|Rule", meta = (DisplayName = "Count", ClampMin = "0",
		EditCondition = "HasSources && (Rule == EBangoTriggerRule::AtLeast || Rule == EBangoTriggerRule::AtMost)", EditConditionHides))
	int32 RuleCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|Rule", meta = (DisplayName = "Min", ClampMin = "0",
		EditCondition = "HasSources && Rule == EBangoTriggerRule::Between", EditConditionHides))
	int32 RuleMin = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|Rule", meta = (DisplayName = "Max", ClampMin = "0",
		EditCondition = "HasSources && Rule == EBangoTriggerRule::Between", EditConditionHides))
	int32 RuleMax = 1;

	/**
	 * This trigger's own constants: every one of its scripts, and its Condition, can read them as inputs, after the
	 * built-in ones (`_red`, `_blue`). For values that differ from one trigger to the next, kept in one place. Each is
	 * a name, a type and a value, edited as a script variable is.
	 */
	UPROPERTY(EditAnywhere, Category = "Trigger|Constants")
	TArray<FBangoTriggerConstant> Constants;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|Timers")
	FBangoTriggerTimer ActivationTimer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|Timers")
	FBangoTriggerTimer DeactivationTimer;

	/** Run when actors join the actual set (_activators). Can read _trigger, _activators, _actual and _potential ("Activator" inputs). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|Scripts", meta = (DisplayName = "On Activator In", EditCondition = "HasSources", EditConditionHides))
	FBangoInlineScript ActivatorInScript;

	/** Run when actors leave the actual set (_activators; destroyed ones are nil). Can read _trigger, _activators, _actual and _potential ("Activator" inputs). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|Scripts", meta = (DisplayName = "On Activator Out", EditCondition = "HasSources", EditConditionHides))
	FBangoInlineScript ActivatorOutScript;

	/**
	 * Run when the trigger activates. Can read _trigger, _activators, _actual and _potential;
	 * _deactivatedScriptHandle: On Deactivated's id while it's still running, else 0, so it can be stopped with Abort;
	 * and _initial: true when an Undetermined trigger is settling into its first state (snap, don't animate).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|Scripts", meta = (DisplayName = "On Activated"))
	FBangoInlineScript ActivatedScript;

	/**
	 * Run when the trigger deactivates. Can read _trigger, _activators, _actual and _potential;
	 * _activatedScriptHandle: On Activated's id while it's still running, else 0, so it can be stopped with Abort;
	 * and _initial: true when an Undetermined trigger is settling into its first state (snap, don't animate).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|Scripts", meta = (DisplayName = "On Deactivated"))
	FBangoInlineScript DeactivatedScript;

	// ---- Events ----

	UPROPERTY(BlueprintAssignable, Category = "Trigger")
	FBangoTriggerEvent OnActivatorIn;

	UPROPERTY(BlueprintAssignable, Category = "Trigger")
	FBangoTriggerEvent OnActivatorOut;

	UPROPERTY(BlueprintAssignable, Category = "Trigger")
	FBangoTriggerStateEvent OnActivated;

	UPROPERTY(BlueprintAssignable, Category = "Trigger")
	FBangoTriggerStateEvent OnDeactivated;

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

	/** The last On Activated script started (it may have ended since). Invalid if none has started. */
	UFUNCTION(BlueprintPure, Category = "Bango|Trigger")
	FBangoScriptHandle GetActivatedScriptHandle() const { return ActivatedScriptHandle; }

	/** The last On Deactivated script started (it may have ended since). Invalid if none has started. */
	UFUNCTION(BlueprintPure, Category = "Bango|Trigger")
	FBangoScriptHandle GetDeactivatedScriptHandle() const { return DeactivatedScriptHandle; }

	/** The actual set, in the order they joined. */
	UFUNCTION(BlueprintPure, Category = "Bango|Trigger")
	TArray<AActor*> GetActualActivators() const;

	/** The potential set, from the last check. */
	UFUNCTION(BlueprintPure, Category = "Bango|Trigger")
	TArray<AActor*> GetPotentialActivators() const;

	/** Everyone gathered at the last check. */
	UFUNCTION(BlueprintPure, Category = "Bango|Trigger")
	TArray<AActor*> GetGatheredActors() const;

	/** Activates now (counts as an activation) if it isn't active or finished (an Undetermined one too, not as initial). */
	UFUNCTION(BlueprintCallable, Category = "Bango|Trigger")
	void ForceActivate();

	/** Deactivates now if it's active, or Undetermined (not as initial). */
	UFUNCTION(BlueprintCallable, Category = "Bango|Trigger")
	void ForceDeactivate();

	/**
	 * Back to the Start State with no activations and empty sets. Nothing is run now; an Undetermined trigger settles
	 * again at its next check, running On Activated or On Deactivated.
	 */
	UFUNCTION(BlueprintCallable, Category = "Bango|Trigger")
	void ResetTrigger();

	// ---- For the subsystem, sources, regions and the editor ----

	/** Called every frame by UBangoTriggerSubsystem, with the world's game time. */
	void TickTrigger(double Now);

	/** Where the regions are placed from: the actor's transform (its root component's). Identity with no actor. */
	FTransform GetAnchorTransform() const;

	/** True if a world location is inside any region. */
	bool IsInsideRegions(const FVector& Location) const;

	/** "Actor.Component", for errors and the debugger. */
	FString GetTriggerName() const;

	/** Redraws the regions in the editor, after changing them from code. Nothing is drawn in game. */
	void RegionsChanged();

	/**
	 * Gives the scripts and the Condition the Constants as their own inputs (SetInstanceInputs). Done by itself when the
	 * trigger is made, loaded, edited or begins play; call it after changing Constants from code before then. False,
	 * with the first error in OutError, if they can't be used (a name that's wrong or taken): the scripts then don't
	 * compile, with that as their error.
	 */
	bool UpdateConstants(FString* OutError = nullptr);

	// UObject
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;

	// UActorComponent
	virtual void OnRegister() override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
	virtual void PostEditImport() override;
#endif

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// For EditCondition: with no sources, only the Condition decides, so who's-where settings are hidden.
	UFUNCTION()
	bool HasSources() const { return !Sources.IsEmpty(); }
	UFUNCTION()
	bool ShowRegions() const { return !Sources.IsEmpty() || !Regions.IsEmpty(); }

#if WITH_EDITORONLY_DATA
	/** Draws the regions in editor worlds (never in game): transient, made by OnRegister, on the same actor. */
	UPROPERTY(Transient, DuplicateTransient, TextExportTransient)
	TObjectPtr<UBangoTriggerShapeComponent> ShapeComponent;
#endif

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
	/**
	 * bInitial: an Undetermined trigger settling (the scripts' _initial). Not named Activate and Deactivate: those are
	 * UActorComponent's (virtual) component activation, which a trigger ignores.
	 */
	void ActivateTrigger(bool bInitial);
	void DeactivateTrigger(bool bInitial);
	EBangoTriggerState GetStartingState() const;
	/** True if the trigger is still running normally after something that may have changed it (a script, an event). */
	bool IsStillRunning() const;

	/**
	 * The "Activator" inputs, in order: _trigger, _activators, _actual, _potential. "Activated" and "Deactivated" add
	 * the other script's id and _initial.
	 */
	TArray<FBangoValue> MakeEventArgs(const TArray<FChangedActor>& Changed) const;
	/** Handle's id while it's running, else 0. */
	int32 RunningScriptId(FBangoScriptHandle Handle) const;
	/** Starts Script with Args; gives its handle (invalid if it didn't start). */
	FBangoScriptHandle SpawnScript(const FBangoTriggerScript& Script, const TArray<FBangoValue>& Args) const;
	/** Runs an activator event: its script, then its Blueprint event. */
	void RunActivatorEvent(const FBangoTriggerScript& Script, FBangoTriggerEvent& Event, const TArray<FChangedActor>& Changed);
	/** False once a script has destroyed this component: its Blueprint events aren't fired then. */
	bool CanBroadcast() const;
	static TArray<AActor*> ToActors(const TArray<FChangedActor>& Changed);

	/** The Constants' values, in order, worked out when it begins play; passed after each script's other inputs. */
	TArray<FBangoValue> ConstantValues;
	/** The same, by name, for the Condition's watch group. */
	TArray<FBangoWatchInput> ConstantInputs;
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
	/** The last On Activated and On Deactivated scripts started, for each other's inputs. */
	FBangoScriptHandle ActivatedScriptHandle;
	FBangoScriptHandle DeactivatedScriptHandle;
};
