#pragma once

#include "CoreMinimal.h"

/**
 * The Bango Triggers show flag (spec section 12, Debugging): while it's on in a viewport, every trigger in that world
 * is drawn there, over everything. Show > Developer > Bango Triggers in the editor and in play-in-editor, or
 * `ShowFlag.BangoTriggers 1` in the console. Not in shipping builds.
 *
 * While playing: each trigger's regions in its state's colour, a line to each actor it has gathered (green: actual;
 * red: potential, but its Condition is false; grey: not in the right place), and a label with its name, state, Rule,
 * timer and activations. In the editor, where the regions are drawn anyway: just the label, with its Start State.
 *
 * Drawn by the debug draw service on the viewport's canvas, so a trigger gets nothing added and costs nothing while the
 * flag is off.
 */
namespace BangoTriggerDebugDraw
{
	/** Called by the module. */
	void Register();
	void Unregister();
}
