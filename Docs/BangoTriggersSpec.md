# Bango Triggers Spec (draft)

A **Bango Trigger** watches for actors (**who**), in places (**where**), that pass a **Condition**. When its rule is met it **activates**; when the rule stops being met it **deactivates**. Along the way it reports each actor that starts or stops counting. Each of these runs a Bango inline script and fires a Blueprint event.

A trigger is a component, **`UBangoTriggerComponent`** (**Bango Trigger** in the Add Component list), configured entirely by its settings. It goes on the thing it's about: a door that opens when a player comes near has a trigger component, whose scripts open and close the door (`self`). An actor can have several. For places that aren't any one thing ("a player reaches the exit"), **`ABangoTrigger`** is an actor with nothing but a trigger component and an editor icon. A new trigger starts as "a player enters this area": one **Players** source and one square **Area** region. It's made for a 3D isometric game and inspired by Arma's triggers.

It's a plain actor component, not a scene component, so moving its actor costs it nothing: it reads where its actor is only when it checks (section 3). Its scripts belong to its actor, which is `self` in them; `_trigger` is the component (section 8).

Bango Triggers depends on the **BangoScripts** plugin (`Plugins/BangoScripts/Docs/BangoSpec.md`) for conditions, scripts, inputs signatures and Bango IDs.

---

## 1. Overview: one check

The trigger keeps three sets of actors up to date:

| Set | Made from | Meaning |
|---|---|---|
| **Gathered** | the **Sources** (section 2), at each check | Everyone the trigger is about: "the players", "the barrels", "@john and @mary". |
| **Potential** | gathered actors that pass the **Presence** test against the **Regions** (sections 3, 4), at each check | Those in the right place: inside the trigger (Present) or outside it (Not Present). |
| **Actual** | potential actors whose **Condition** is true, kept up to date by watching it (section 5) | Those that count: inside the cloud *and* not wearing a gasmask. |

A **check** happens every **Poll Interval** (section 9): it works out who's where. The Condition isn't run at every check: Bango watches it for each potential actor and says when an answer changes.

Whenever the actual set changes:
1. Actors that joined or left it run **On Activator Out** and **On Activator In** (section 7).
2. The **Rule** (section 6) is tested on the sets. It switching from not met to met (after the Activation Timer, if any) **activates** the trigger: **On Activated**. It switching back **deactivates** it: **On Deactivated** (section 7).

A trigger with **no sources** has no actors: its Condition is watched once, and the trigger is active while it's true (section 5). With **no sources and no Condition**, it's a **manual** trigger: only Force Activate and Force Deactivate change it (section 5).

---

## 2. Sources: who

**Sources** is a list. Each source returns actors; the gathered set is all of them together, each actor once, in source order (and each source's own order). The trigger's own actor, and actors being destroyed, are never gathered: a door doesn't open for itself.

Built-in sources:

| Source | Settings | Returns |
|---|---|---|
| **Actor Class** | Classes (a list) | Every actor of those classes, or their subclasses. |
| **Bango IDs** | Names (a list, without `@`) | The actors with those Bango IDs, looked up in the Bango registry at each check. IDs not registered yet return nothing. |
| **Players** | — | Every pawn controlled by a player. |
| **Local Players** | — | The pawns of this machine's local players (split-screen: several). On a server, only the host's own. |

Custom sources are C++ structs that game projects add (section 10).

Sources are gathered fresh at each check. (Sharing one source's results between triggers checking in the same frame is a later idea, section 13.)

---

## 3. Regions: where

**Regions** is a list of shapes, placed relative to the trigger's **anchor**, its actor's transform (its root component's): they move, turn and scale with the actor. Below, "the trigger's" location, rotation or scale means the anchor's. A trigger whose actor has no root component, but has regions, reports an error and doesn't run. The anchor is always the actor's root: a trigger can't follow one of its actor's parts (a door that swings on a hinge should have its trigger on an actor, or a root, that doesn't). An actor is **inside the trigger** if its **location** (`GetActorLocation()`, its centre point, never its collision) is inside **any** region.

Built-in regions:

| Region | Settings | Inside if |
|---|---|---|
| **Area** | Shape (Polygon, Rectangle, Ellipse); Points (Polygon) or Size (Rectangle, Ellipse); Offset; Yaw; Display Height | Seen from above, the location's X and Y are inside the shape: it has **infinitely high walls**. Only the trigger's location, yaw and X/Y scale apply. A polygon needs at least 3 points and uses the even-odd rule if it crosses itself. |
| **Box** | Offset; Rotation; Extent (half size) | The location is inside the box. |
| **Sphere** | Offset; Radius | The location is within Radius of the centre (scaled by the trigger's smallest scale). |
| **Capsule** | Offset; Rotation; Radius; Half Height | The location is inside the capsule. |

A trigger can mix them: a polygon area for a courtyard plus a box for a balcony above it. Custom regions are C++ structs (section 10).

---

## 4. Presence

**Presence** compares each gathered actor with the regions:
- **Present**: potential if it's inside the trigger.
- **Not Present**: potential if it's outside every region.

With **no regions** there's no "where": every gathered actor is potential, and Presence isn't used.

Example: "activate when the player isn't in the shop": Source **Local Players**, Region the shop's floor, Presence **Not Present**.

---

## 5. Condition

**Condition** is a Bango inline condition (`FBangoInlineCondition`, BangoSpec section 15) that answers `true` or `false`: one expression (`!_activator HasItem("Gasmask")`) or a script whose main graph returns a bool. Empty means always `true`. The editor checks it as a condition.

### Watching
The trigger doesn't run its Condition at every check. It **watches** it (BangoSpec section 10, Watching conditions):
- When play begins, the trigger makes one **watch group** for its Condition. It's compiled then: a Condition that doesn't compile shows its error, and is never true.
- When an actor **becomes potential**, the trigger starts a **watch** for it, with `_activator` set to that actor. Its first answer comes straight away: if it's `true`, the actor is actual at once.
- When an actor **stops being potential**, its watch is stopped (and, if it was actual, it leaves the actual set).
- The group's shared inputs, `_trigger`, `_potential` and `_actual` (section 8), are updated whenever those sets change.
- Bango checks a watch again only when its answer might have changed:
  - a **global** it reads has changed;
  - a group input it reads (`_potential`, `_actual`) has changed;
  - or, if it calls a **command** or uses **`call`** (whose answers can change without Bango seeing anything change), every **Poll Interval** of its own. That's a setting on the Condition, shown in the details panel only for a Condition that needs it; by default it's the project setting **Default Condition Poll Interval** (0.5 s).
- Answers that changed are reported at the **end of the frame**; the actual set and the events follow (section 7).

So a gasmask Condition that reads a global is checked only when that global changes; one that calls `HasItem` is polled, at its own Poll Interval.

### Answers and errors
- Only `true` counts. `false`, **Unset** (it read a global that doesn't exist yet or is `nil`) and errors don't.
- An error is shown on screen once for each watch, and again only after it has answered `true` or `false` in between.
- It can't wait (`Sleep`).

### With no sources
There are no actors, so the trigger watches its Condition once, with `_activator` `nil`. The trigger is active while it's `true` (the Rule isn't used), and On Activator In and Out never run.

### Manual triggers
With no sources **and an empty Condition**, a trigger is **manual** (**Is Manual Trigger**): it never checks or watches anything, and nothing activates it but **Force Activate**, and nothing deactivates it but **Force Deactivate** (from Blueprint, C++, or a script: `_trigger call ForceActivate();`). For a door opened by a lever's script, say.
- Its timers aren't used: Force Activate and Force Deactivate never wait.
- With Start State **Undetermined** it settles once, at its first check, as **Deactivated** (On Deactivated runs, `_initial` true): nothing is met. Then it waits to be forced.
- For a trigger that's simply always active instead, give it the Condition `true`.

### A Condition that reads `_actual`
A Condition can depend on the actual set it decides, like "only while fewer than 3 are already poisoned". Each change to `_actual` checks every watch that reads it again, at the end of the next frame. Bango does at most one round of checks per frame, so this can't loop forever, but the set can **flicker**: actors joining, which makes others leave, which lets them join again. Avoid it unless it's what you want.

---

## 6. Rule

The **Rule** says when the trigger is active. It's tested only when there's **at least one potential actor**: with none, the trigger isn't active, whatever the Rule.

| Rule | Settings | Met when |
|---|---|---|
| **All** | — | Every **gathered** actor is actual. |
| **At Least** | N (default 1) | At least N actors are actual. |
| **At Most** | N | At most N actors are actual (0 included). |
| **Between** | Min, Max | Between Min and Max actors (inclusive) are actual. |

The number boxes show only for the rules that use them.

**All** counts gathered actors, not just potential ones, so it means "everyone the trigger is about":
- "Both heroes reach the exit": Sources **Bango IDs** `hero_a`, `hero_b`; Region the exit; **Present**; **All**. A hero not yet registered isn't gathered, so isn't required.
- "No player is in the area": Sources **Players**; Region the area; **Not Present**; **All**. A player inside is gathered but not potential, so All isn't met.

---

## 7. Events

### Activators in and out
The actual set changes at two moments:
- at a **check**, when actors become or stop being potential (a new potential actor whose first answer is `true` joins at once; one that stops being potential leaves);
- at the **end of a frame**, when Bango reports Condition answers that changed.

Each time, the trigger compares the set with what it was:
- **On Activator Out** runs once for all the actors that **left** the actual set: their Condition failed, or they stopped being potential (left the area, or stopped being gathered). An actor **destroyed** while actual leaves too, as `nil`.
- **On Activator In** runs once for all the actors that **joined** it.

Out runs before In. Each runs only if something joined or left, and gets those actors as `_activators` (section 8): usually one, but ten barrels spawned inside together join together.

### Activated and deactivated
The Rule is tested after every check and every change, after In and Out (the gathered and potential sets matter too: with **All**, a newly gathered actor can stop the Rule being met without anyone leaving the actual set):
- The Rule becoming met starts the **Activation Timer** (section 9). When the timer ends (or at once, with no timer), the trigger **activates**: **On Activated** runs.
- The Rule stopping being met while active starts the **Deactivation Timer**; when it ends, the trigger **deactivates**: **On Deactivated** runs.
- For these, `_activators` is the actors that joined or left in the change that made the Rule change, which is what changed it. With no sources it's empty.

For each change, the order is always: **Out, In, then Activated or Deactivated.**

The trigger activates only on a change from not met to met: someone standing in the area doesn't activate it again and again.

### Start State
**Start State** says what the trigger is when play begins:
- **Deactivated** (default): it starts idle. Nothing runs until the Rule is first met, so if it isn't met at the start, On Deactivated never runs.
- **Undetermined**: for things whose own starting state isn't known, like a light whose Blueprint picks whether it starts on, switched by the trigger. The trigger starts **Undetermined** (neither active nor idle: **Is Trigger Active** is false) and **settles** at its first check, which happens on the first frame of play, whatever the Poll Interval:
  - **Rule met**: the actors there run **On Activator In**, then the trigger activates at once: **On Activated** runs, `_activators` being those actors. It counts as an activation (Max Activations).
  - **Rule not met**: it deactivates at once: **On Deactivated** runs, `_activators` empty. The light goes off.
  - Either way the timers are skipped: it's meant to be in its right state from the start, not to correct itself after a timer. From then on the trigger works as usual, timers included.
  - Its scripts get `_initial` true, and its Blueprint events **Initial** true, for this first state only (section 8), so a door that starts open can snap open instead of playing its opening animation and sound.
  - A trigger that starts disabled stays Undetermined until it's enabled, then settles at its next check.
  - **Reset Trigger** goes back to Undetermined, and it settles again at its next check.
  - **Force Activate** and **Force Deactivate** work on an Undetermined trigger too, but aren't its initial state (`_initial` is false).

Settling happens at the first check, not in BeginPlay itself, because other actors may not have begun play yet. Even so, a player's pawn may not exist on the first frame: a "player is near" trigger then settles as Deactivated and activates a moment later.

### Max Activations
**Max Activations** (default 0, no limit) limits how many times it can activate. After its last activation it still runs as usual (In, Out, and its deactivation), then it's **Finished**: it stops checking and runs nothing more.

---

## 8. Script inputs

Every script can read these inputs directly, without an `#inputs` line (they're inputs signatures, BangoSpec section 15). `UBangoTriggerComponent`'s constructor registers the signatures and gives them to the scripts, so the editor knows them (autocomplete and checking).

The scripts belong to the trigger's actor: **`self`** is the actor (the door). **`_trigger`** is the component itself (BangoSpec section 4, Entity IDs: components). It's an entity like any other, but a component without a place in the world, so commands that place or move things (`SetPos`, `SetHidden`) don't work on it. **`call`** does: `_trigger call SetTriggerEnabled(false);` (section 11). A trigger made by its actor's class, Blueprint or level is the same entity each time (`door_1|Trigger`); one added while playing gets an Automatic ID.

**On Activator In, On Activator Out** (the "Activator" signature), **On Activated** ("Activated") and **On Deactivated** ("Deactivated"):

| Input | Type | Value |
|---|---|---|
| `_trigger` | `entity` | This trigger: the component. Its actor is `self`. |
| `_activators` | `entity[]` | The actors this event is about (section 7). A destroyed actor is `nil`. |
| `_actual` | `entity[]` | The actual set after the change, in the order they joined. |
| `_potential` | `entity[]` | The potential set. |
| `_deactivatedScriptHandle` | `int` | On Activated only: the id of the last On Deactivated script, if it's still running; otherwise `0`. |
| `_activatedScriptHandle` | `int` | On Deactivated only: the id of the last On Activated script, if it's still running; otherwise `0`. |
| `_initial` | `bool` | On Activated and On Deactivated only: `true` when an Undetermined trigger is settling into its first state (section 7, Start State), as play begins or after a reset; otherwise `false`. |

The two handles let On Activated and On Deactivated, which may be running latent commands, stop each other: `Abort(_deactivatedScriptHandle);` (BangoSpec section 7). A handle is never `nil`, because `Abort(nil)` would stop the script calling it; `Abort(0)` does nothing, so no check is needed. The handle is that of the script started by the trigger's most recent other event. The new script gets it as soon as it starts, before its first command runs.

```
// On Deactivated: stop the alarm loop On Activated started, then fade out.
Abort(_activatedScriptHandle, true);
self FadeOutAlarm();
```

```
// On Activated, on a door whose trigger's Start State is Undetermined: open without fuss if it starts open.
if (_initial)
{
	self SnapOpen();
}
else
{
	self Open();
}
```

**Condition** (the "Condition" signature):

| Input | Type | Value |
|---|---|---|
| `_trigger` | `entity` | As above. |
| `_activator` | `entity` | The actor being checked; `nil` with no sources. |
| `_actual` | `entity[]` | The actual set, as it last changed. |
| `_potential` | `entity[]` | The potential set, from the last check. |

```
// On Activator In, for a cloud of sickness whose Condition is: !_activator HasItem("Gasmask")
forEach (_a in _activators)
{
	_a AddStatus("Poisoned");
}
```

```
// On Activator Out
forEach (_a in _activators)
{
	if (IsNil(_a))
	{
		self Say("Bye!");
		continue;
	}
	_a RemoveStatus("Poisoned");
}
```

- Use `Count(_actual)` for how many count.
- Every actor in a game world is an entity, with or without a Bango ID component (BangoSpec section 4, Entity IDs), so no actor is `nil` in these arrays unless it's been destroyed.

### Constants
A trigger's **Constants** list holds values of its own, which **all its scripts and its Condition** can read as inputs, after the ones above. They're for values that differ from one trigger to the next and that several of its scripts share, kept in one place: a light trigger fading between `_red` and `_blue` in both On Activated and On Deactivated.

```
// On Activated:   self call FadeColor(_red, _blue, 2.0);
// On Deactivated: self call FadeColor(_blue, _red, 2.0);
```

- Each is edited on **one row**, as a script variable is in the script editor's SCRIPT VARS list, with the same widgets (BangoSpec section 15, Value widgets): its **Name** (`_red`; it should start with `_`, like the built-in inputs), its **Type** (the type picker: every type a value can have, and an array of each), and its **Value**, typed as a graph node's field is: a string or text as plain words, anything else as code (`[1, 0, 0]`, `@lamp`, `[@a, @b]`, `true`), checked as it's typed. Empty is `nil`. Each is edited on its own: picking another type clears the value (as SCRIPT VARS does; `nil` stays), and never changes the name.
- **An `entity`** is the one exception: its value is an **actor picker** (with the eyedropper), for an actor in the same level, so an actor without a Bango ID can be used. None is `nil`. An `entity[]` is typed like other arrays, with Bango IDs (`[@lamp1, @lamp2]`).
- A **text** typed as plain words is `INVTEXT`: it isn't translated yet (that waits on BangoScripts gathering texts kept outside scripts).
- They're the scripts' **instance inputs** (BangoSpec section 15, Instance inputs): the editor knows them as you type (autocomplete, colouring, errors, the SCRIPT INPUTS list), and follows them as they're edited, without the scripts being edited.
- A name a built-in input has (`_trigger`, `_initial`, ...), or another constant's, is an error, shown as the error of each script that has that input (in the editor, and when it begins play) until it's renamed: a constant called `_initial` breaks On Activated and On Deactivated, whose input that is, but not the others. Rename it rather than rely on that.
- Their values are worked out when the trigger begins play (`Bango::ParseFixedValue`, as a script variable's starting value is read), and never change while it runs: a picked actor's entity is the actor's then (its Bango ID, or the ID it's given). A value that can't be read is reported then, and is `nil`. A Condition that reads them isn't polled for them.
- In C++, after changing `Constants` before it begins play, call `UpdateConstants()` so the scripts get the new list (it's done by itself when the trigger is made, loaded, edited, undone, pasted, restored after its actor's construction script reruns, and as it begins play). Each is an `FBangoTriggerConstant`: `Name`, `Type`, `Value` (the Bango text) and `Actor` (an `entity`'s).

---

## 9. Timing, timers and where it runs

### Checking
Nothing tells a trigger when actors move, so triggers **check** who's where every **Poll Interval** seconds (default `0.5`; `0` checks every frame): they gather their sources and test the regions. One subsystem checks every trigger, spreading the checks across frames. Checks use game time: none while the game is paused. An actor that crosses a small region between two checks isn't seen.

The Condition isn't run at checks: it's watched (section 5), and has a Poll Interval of its own, used only when it calls commands.

### Timers
**Activation Timer** and **Deactivation Timer** apply to the trigger activating and deactivating (not to In and Out):
- **Mode**: **None** (at the check that finds it), **Timeout** (the Rule must stay met, or unmet, for the whole time, or the timer is cancelled), or **Countdown** (once started it runs to the end, whatever happens).
- **Min Time**, **Max Time** (seconds): picked at random between them each time it starts.
- A running timer is looked at every frame, so it ends on time, not at the next check.

### Runs On
**Runs On** says where the trigger checks:
- **Server** (default): standalone games, listen servers and dedicated servers.
- **Client**: only on clients that aren't the server.
- **Anywhere**: both.

Elsewhere it's inert. Multiplayer is a goal, not yet tested (section 13).

---

## 10. C++: custom sources and regions

Sources and regions are instanced structs (`TInstancedStruct`), picked from a list in the details panel. A game module adds its own by subclassing:

```cpp
USTRUCT(meta = (DisplayName = "Enemies"))
struct FMyEnemiesSource : public FBangoActivatorSource
{
	GENERATED_BODY()

	virtual void Gather(const UBangoTriggerComponent& Trigger, TArray<AActor*>& OutActors) const override;
};
```

```cpp
USTRUCT(meta = (DisplayName = "Cone"))
struct FMyConeRegion : public FBangoTriggerRegion
{
	GENERATED_BODY()

	// Location in the world; TriggerTransform is the trigger's anchor (its actor's transform).
	virtual bool IsInside(const FTransform& TriggerTransform, const FVector& Location) const override;
	// For the editor: its bounds, the lines it's drawn with (world space), and where its centre handle is.
	virtual FBox GetBounds(const FTransform& TriggerTransform) const override;
	virtual void AddDrawLines(const FTransform& TriggerTransform, TArray<FBangoTriggerLine>& OutLines) const override;
	virtual FVector GetCentre(const FTransform& TriggerTransform) const override;
};
```

A custom region gets a centre handle in the editor; moving it with the gizmo is only built in for the built-in regions.

`Gather` may return duplicates, the trigger's own actor or invalid actors: the trigger removes them.

---

## 11. Blueprint API

Events (multicast delegates on the trigger component), each fired just after its script starts, with the trigger component and the same actors as `_activators` (destroyed ones as none). In a door Blueprint they're the component's events in its event graph ("On Activated (DoorTrigger)"):
- **On Activator In**, **On Activator Out**.
- **On Activated**, **On Deactivated**: these also have **Initial**, as the scripts' `_initial` (section 7, Start State).

Functions:
- **Set Trigger Enabled** (bool), **Is Trigger Enabled**: a disabled trigger doesn't check, and its timers pause; it keeps its state. **Start Enabled** sets it when play begins.
- **Get Trigger State**: Undetermined (not settled yet, section 7), Idle, Activating (Activation Timer running), Active, Deactivating (Deactivation Timer running) or Finished. **Is Trigger Active**: Active or Deactivating.
- **Get Activation Count**.
- **Get Activated Script Handle**, **Get Deactivated Script Handle**: the last On Activated and On Deactivated scripts started (they may have ended since); for **Terminate Bango Script** or **Is Bango Script Running**. Invalid if none has started.
- **Get Actual Activators**, **Get Potential Activators**, **Get Gathered Actors**: the sets from the last check.
- **Is Manual Trigger**: no sources and an empty Condition (section 5, Manual triggers).
- **Force Activate**: activates now (counts as an activation) if it isn't active or finished; `_activators` is empty.
- **Force Deactivate**: deactivates now if it's active or Undetermined.
- **Reset Trigger**: back to its Start State (Idle, or Undetermined) with no activations and empty sets. Nothing is run now; an Undetermined trigger settles again at its next check.

These are the component's own, so scripts can use them with `call` (`_trigger call ForceDeactivate();`). They're named apart from the engine's component **activation** (`Activate`, `Deactivate`, `Is Active`, **Auto Activate**, which are hidden in the details panel): those mean the component is running, not that the trigger is active, and a trigger ignores them.

---

## 12. Editor

- A trigger draws all its regions in editor viewports: the level's, and a Blueprint's. Area regions are drawn as an outline on the ground with walls up to their **Display Height**. The drawing is done by a component the trigger adds to its actor in the editor only, like the engine's editor icons: it isn't saved, isn't listed among the actor's components, and is never made in game, so it costs nothing there. `ABangoTrigger` also shows an editor icon.
- Select the trigger's actor to see its regions' handles:
  - **Every region** has a **centre handle**: drag it with the gizmo to move the region (its Offset). The rotate gizmo turns an area (its Yaw), a box or a capsule (its Rotation). Right-click: **Reset Rotation**.
  - **Area polygons**: click a point to select it and move it with the gizmo (only X and Y); **Delete** removes it (at least 3 stay). Right-click a point: **Add Point After**, **Delete Point**, **Add Points By Clicking** (each click in the level adds a point after the selected one, snapped to the grid if snapping is on; **Esc** or **Enter** stops). Right-click an edge: **Insert Point Here**.
  - **Area rectangles and ellipses**: size handles at the middle of each side and each corner (for an ellipse, its bounding box's): drag one and that side or corner moves, the opposite one staying put (the region's Offset moves by half as much). A **rotate handle** beyond the +X side turns the region (its Yaw), snapped to the rotation grid if rotation snapping is on. Right-click: **Reset Rotation**.
  - **Box, sphere and capsule** sizes are set in the details panel for now (size handles: section 13).
- Every change can be undone.
- **Start State** is shown as a segmented control: **Deactivated** | **Undetermined**, the one it's set to picked. Undetermined is yellow when picked (it will run a script as play begins); Deactivated is the usual grey.
- **With no sources**, the details panel hides what only matters for who's where: **Poll Interval**, **Presence**, **Rule** (and its Count, Min and Max), and the **On Activator In** and **On Activator Out** scripts. **Regions** is hidden too, unless it still has regions, so they can be seen and removed. Hidden values are kept, and come back when a source is added.

### Debugging
The **Bango Triggers** show flag (Show > Developer > Bango Triggers in the editor and in play-in-editor, or `ShowFlag.BangoTriggers 1` in the console; not in shipping builds) draws every trigger in the viewport's world, over everything:
- **While playing**, each trigger's regions in its state's colour: grey Idle, yellow Undetermined, orange Activating, green Active, blue Deactivating, dark Finished. A disabled trigger, or one that isn't running here (Runs On), is faint.
- A line from the trigger to each actor it has gathered: **green**, actual; **red**, potential but its Condition is false; **grey**, not in the right place.
- A label over it: its name; its state (and "disabled"); with sources, how many are actual, potential and gathered, and its Rule and whether it's met (with none, whether the Condition is; "Manual" for a manual trigger); a running timer's time left ("Activates in 1.4s", "paused" while disabled); its activations ("Activations 1 / 3").
- **In the editor**, where the regions are drawn anyway: just a label with its name and Start State.
- It's drawn by the debug draw service on the viewport's canvas: a trigger gets nothing added, and it costs nothing while the flag is off.

---

## 13. Later
- **Sharing source results** between triggers that check in the same frame.
- **Tags**: an actor tag filter, or a Tag source.
- **More debugging tools**: each trigger's cost (each potential actor has a watch, and a polled Condition runs for every one, so a broad source with Not Present can be costly) in a `stat` group and per trigger; console commands to list, log and force triggers; Visual Logger recording; warnings for likely mistakes before playing (Map Check); a Gameplay Debugger category.
- **Limits on In**, in total or per actor ("the cloud poisons 5 people").
- **A per-actor delay before In** ("must breathe the cloud for 2 seconds").
- **Multiplayer**, tested properly.
- **Saving and loading** triggers' state with the game: state, activation count, timers, and the potential and actual sets (including actors gone since).
- **Bango commands** to control triggers from scripts, shorter than `call` (`_trigger EnableTrigger(false);`, or by the actor: `@gate EnableTrigger(false);`).
- **Event sources**: activating from game events (the event plugin).
- **Size handles** for Box, Sphere and Capsule regions, and optional bottom and top heights for Area regions.
