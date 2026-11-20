# Bango Triggers Spec (draft)

A **Bango Trigger** is an actor placed in a level that watches for actors (**who**), in places (**where**), that pass a **Condition**. When its rule is met it **activates**; when the rule stops being met it **deactivates**. Along the way it reports each actor that starts or stops counting. Each of these runs a Bango inline script and fires a Blueprint event.

There is one trigger class, `ABangoTrigger`, configured entirely by its settings. A new one starts as "a player enters this area": one **Players** source and one square **Area** region. It's made for a 3D isometric game and inspired by Arma's triggers.

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

A trigger with **no sources** has no actors: its Condition is watched once, and the trigger is active while it's true (section 5).

---

## 2. Sources: who

**Sources** is a list. Each source returns actors; the gathered set is all of them together, each actor once, in source order (and each source's own order). The trigger itself, and actors being destroyed, are never gathered.

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

**Regions** is a list of shapes, placed relative to the trigger: they move, turn and scale with it. An actor is **inside the trigger** if its **location** (`GetActorLocation()`, its centre point, never its collision) is inside **any** region.

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

### Max Activations
**Max Activations** (default 0, no limit) limits how many times it can activate. After its last activation it still runs as usual (In, Out, and its deactivation), then it's **Finished**: it stops checking and runs nothing more.

---

## 8. Script inputs

Every script can read these inputs directly, without an `#inputs` line (they're inputs signatures, BangoSpec section 15). `ABangoTrigger`'s constructor registers the signatures and gives them to the scripts, so the editor knows them (autocomplete and checking).

**On Activator In, On Activator Out, On Activated, On Deactivated** (the "Activation" signature):

| Input | Type | Value |
|---|---|---|
| `_trigger` | `entity` | The trigger itself, if it has a Bango ID; otherwise `nil`. |
| `_activators` | `entity[]` | The actors this event is about (section 7). A destroyed actor is `nil`. |
| `_actual` | `entity[]` | The actual set after the change, in the order they joined. |
| `_potential` | `entity[]` | The potential set. |

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
		_trigger Say("Bye!");
		continue;
	}
	_a RemoveStatus("Poisoned");
}
```

- Use `Count(_actual)` for how many count.
- For now, every actor a trigger uses is assumed to have a Bango ID. An actor without one is `nil` in these arrays; giving every actor an ID while the game runs is a job for BangoScripts (section 13).

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

	virtual void Gather(const ABangoTrigger& Trigger, TArray<AActor*>& OutActors) const override;
};
```

```cpp
USTRUCT(meta = (DisplayName = "Cone"))
struct FMyConeRegion : public FBangoTriggerRegion
{
	GENERATED_BODY()

	// Location in the world; TriggerTransform is the trigger's.
	virtual bool IsInside(const FTransform& TriggerTransform, const FVector& Location) const override;
	// For the editor: its bounds, the lines it's drawn with (world space), and where its centre handle is.
	virtual FBox GetBounds(const FTransform& TriggerTransform) const override;
	virtual void AddDrawLines(const FTransform& TriggerTransform, TArray<FBangoTriggerLine>& OutLines) const override;
	virtual FVector GetCentre(const FTransform& TriggerTransform) const override;
};
```

A custom region gets a centre handle in the editor; moving it with the gizmo is only built in for the built-in regions.

`Gather` may return duplicates, the trigger itself or invalid actors: the trigger removes them.

---

## 11. Blueprint API

Events (multicast delegates on the trigger), each fired just after its script starts, with the trigger and the same actors as `_activators` (destroyed ones as none):
- **On Activator In**, **On Activator Out**, **On Activated**, **On Deactivated**.

Functions:
- **Set Trigger Enabled** (bool), **Is Trigger Enabled**: a disabled trigger doesn't check, and its timers pause; it keeps its state. **Start Enabled** sets it when play begins.
- **Get Trigger State**: Idle, Activating (Activation Timer running), Active, Deactivating (Deactivation Timer running) or Finished. **Is Trigger Active**: Active or Deactivating.
- **Get Activation Count**.
- **Get Actual Activators**, **Get Potential Activators**, **Get Gathered Actors**: the sets from the last check.
- **Force Activate**: activates now (counts as an activation) if it isn't active or finished; `_activators` is empty.
- **Force Deactivate**: deactivates now if it's active.
- **Reset Trigger**: back to Idle with no activations and empty sets. Nothing is run.

---

## 12. Editor

- The trigger shows an editor icon and draws all its regions, in the level viewport only (not in game). Area regions are drawn as an outline on the ground with walls up to their **Display Height**.
- Select the trigger to see its regions' handles:
  - **Every region** has a **centre handle**: drag it with the gizmo to move the region (its Offset). The rotate gizmo turns an area (its Yaw), a box or a capsule (its Rotation). Right-click: **Reset Rotation**.
  - **Area polygons**: click a point to select it and move it with the gizmo (only X and Y); **Delete** removes it (at least 3 stay). Right-click a point: **Add Point After**, **Delete Point**, **Add Points By Clicking** (each click in the level adds a point after the selected one, snapped to the grid if snapping is on; **Esc** or **Enter** stops). Right-click an edge: **Insert Point Here**.
  - **Area rectangles and ellipses**: size handles at the middle of each side and each corner (for an ellipse, its bounding box's): drag one and that side or corner moves, the opposite one staying put (the region's Offset moves by half as much). A **rotate handle** beyond the +X side turns the region (its Yaw), snapped to the rotation grid if rotation snapping is on. Right-click: **Reset Rotation**.
  - **Box, sphere and capsule** sizes are set in the details panel for now (size handles: section 13).
- Every change can be undone.

---

## 13. Later
- **Runtime Bango IDs** (BangoScripts): every actor a trigger uses should be usable in scripts, with or without a hand-placed Bango ID.
- **Sharing source results** between triggers that check in the same frame.
- **Tags**: an actor tag filter, or a Tag source.
- **Debugging tools**, including showing each trigger's cost (each potential actor has a watch, and a polled Condition runs for every one, so a broad source with Not Present can be costly) and drawing triggers and their sets in game.
- **Limits on In**, in total or per actor ("the cloud poisons 5 people").
- **A per-actor delay before In** ("must breathe the cloud for 2 seconds").
- **Multiplayer**, tested properly.
- **Saving and loading** triggers' state with the game: state, activation count, timers, and the potential and actual sets (including actors gone since).
- **Bango commands** to control triggers from scripts (`@gate EnableTrigger(false);`).
- **Event sources**: activating from game events (the event plugin).
- **Size handles** for Box, Sphere and Capsule regions, and optional bottom and top heights for Area regions.
