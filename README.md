# Part Prac 1.0.4 — Love Studios

Windows Geode mod for Geometry Dash 2.2081 / Geode 5.11.1.

## Install

Copy `lovestudios.part_prac.geode` into Geometry Dash's `geode/mods` folder and restart the game.
On this PC that folder is `C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash\geode\mods`.
The delivered file is not installed automatically. No additional mods are required.

## Use

1. Download the level and its music normally. Hold **Shift** and left-click **Play** on its level information page (or a local level's Play button).
2. Confirm **Enter Part Prac Mode**.
3. In the read-only selector, drag **left to right**. The green point where you start dragging is the player's entry position; the right edge is where the loop ends. Put the entry above the floor, not inside a block. The rectangle's height is a visual guide: it does not crop objects or delete triggers.
4. **Space + drag** pans. The **+ / -** buttons zoom. Select at least one block of horizontal distance.
5. Press **Escape** (or Menu), then **Prac**. The section loops when you reach its right edge. Dying restarts the section too.
6. Pause and quit to return to your selection. Escape > Leave returns to the original level page.

## Music and triggers

The full level stays loaded, including objects, decoration, groups and triggers outside the selected box. The original level and song offsets are kept. Existing Start Pos objects are removed from the temporary practice copy; instant practice inserts one at your chosen entry and uses Geometry Dash's native start-position reset and music timing.

**Prac** infers ordinary mode/speed/mini/gravity/dual portals before the selection. It uses native Start Pos reconstruction for effects. It cannot guarantee the exact state produced by earlier player inputs, route-dependent portals, random triggers or arbitrary custom logic.

For those levels choose **Capture** instead. Play the lead-in yourself once. On first reaching the selected start X, the mod creates a real native practice checkpoint containing the player's state, game state, shader state, effect-manager state, saved objects and audio state. Subsequent loops and deaths restore that checkpoint with practice music sync enabled. Auto/manual checkpoint replacement is disabled during this session. Deaths before capture restart the lead-in. Prac always starts at your selection; Capture is a separate explicit choice.

Capture preserves what Geometry Dash itself saves in a checkpoint; it is not a separate simulator and cannot fix native checkpoint bugs or modifications made by other mods. Capturing mid-jump also preserves that jump's velocity.

## Scope

- Supports forward-moving classic sections. Platformer levels are explicitly rejected. Backtracking, reverse/rotated gameplay and overlapping routes are not reliably represented by this horizontal selector; use a forward-moving section.
- Shift-click is hooked on main-level Play buttons, downloaded-level pages and local-level information pages.
- Temporary copies stay in memory. They are not added to the user's created levels. The original level is not edited, and practice-copy percentage saves are blocked. Test mode is forced, and completion is intercepted to loop.
- The selector is fully released before gameplay loads. On return, it is rebuilt with the selection retained; the camera zoom is restored only after the editor enters its new scene. This avoids accessing incomplete editor state while BetterEdit, Tinker, and Good Grid have hooks installed.
- Custom songs/SFX must already be available locally through the game's normal download flow.

## Verification

The original v1.0.2 source was built with Geode 5.11.1 SDK for GD 2.2081. This v1.0.4 source patch has NOT been rebuilt with the Windows Geode SDK in this environment. Four standalone C++ test suites pass: 12 start-marker validation / level assembly checks, eight range / level-data checks, editor scene teardown and re-entry checks, and exit-relay sequencing checks. A source regression test verifies the selector no longer calls updateZoom inside init. The lifecycle tests use simulated owners; they do not run the live game or Tinker.

**Not yet compiled into a native v1.0.4 Windows DLL or live-tested in Geometry Dash.** In-game input dispatch, checkpoint/audio restoration, menu return and compatibility with installed mods still need a gameplay check. The reported Tinker crash has been addressed by changing the scene lifecycle; it still needs confirmation in the installed mod combination.

Suggested first check: a simple downloaded level with a song, speed portal and earlier move/color trigger. Loop three times, die, return to the selector, and repeat using Capture. Check the original completion percentage remains unchanged. Then try a multi-song/dual level.

## Build

Install CMake 3.25+, Visual Studio C++ tools / Windows SDK, Geode CLI, and the Geode 5.11.1 SDK with its Windows prebuilt library. In a VS x64 development terminal, set GEODE_SDK, then:

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

The output is `build/lovestudios.part_prac.geode`. CMake uses DONT_INSTALL, so building never replaces an installed mod automatically.

References: [Geode mod setup](https://docs.geode-sdk.org/getting-started/create-mod/), [official game bindings](https://github.com/geode-sdk/bindings/tree/main/bindings/2.2081).


## 1.0.1 crash fix

The supplied crash occurred in an editor-only Tinker modifier-icon hook while Part Prac was creating gameplay objects. Version 1.0.0 kept the editor scene alive underneath gameplay and cleared its global pointer. Tinker disables its editor hooks when its editor UI is destroyed, so that transition left its hook active with no editor to use.

Version 1.0.1 replaces the old scene with a small loading scene, waits for both the old layer and UI to be destroyed, and only then constructs the destination. It uses the same ordering when returning to the selector or leaving it. No Tinker settings or files are changed.

Source evidence: [Tinker modifier icon hook](https://github.com/Alphalaneous/Tinker/blob/main/src/modules/AwesomeModifierIcons.cpp), [Tinker editor hook cleanup](https://github.com/Alphalaneous/Tinker/blob/main/src/MainHooks.cpp).

## 1.0.2 selected-start fix

Explicitly assigns object ID 31 and both serialized/runtime position fields to the generated Start Pos. Validates the emitted object ID, coordinates and enabled state before launching. Gameplay refuses to start if the native engine did not activate a Start Pos, returning to the selector with an explanation. Prac now always selects instant practice; only Capture plays the lead-in. The old exact-capture preference is no longer used.

Compilation and automated tests pass. This update has not been tested inside the running game.


## Verification hotfix (15 October 2026)

An in-game log showed Start Pos validation rejecting native `1,31,...,kA21,0`
markers. This build removes the strict float equality guard on serialized X/Y
while preserving marker ID, enabled-state checking and the gameplay-time native
Start Pos presence check. This is a targeted hotfix, **not a live-tested fix**.
The patched `.geode` is derived from the original Windows DLL without an SDK
rebuild; the source includes an additional numeric-coordinate guard for future
rebuilds. Back up the original DLL and retest an ordinary section.


## 1.0.3 exit-crash source patch (unverified in-game)

This patch switches Part Prac's gameplay quit path to the **native** `PlayLayer::onQuit()` routine, so Geometry Dash can run its normal pause/gameplay cleanup. Rather than replace the scene within the quit callback, an inert relay on the original level page waits for the old gameplay layer to be released before pushing a new selector scene. The selector Leave button likewise uses the native pop path and only clears session state after the editor and UI have been destroyed.

`ExitGate` includes a two-frame ready check after the old layer/UI are gone to avoid cross-frame teardown races. A new regression test simulates a late native pop, retained editor UI, interrupted scene activity, and exactly-once return. These are **unit tests, not an in-game crash reproduction**.

**Important:** This ZIP contains patched C++ source, not a working newly compiled Windows `.geode` plugin. The earlier log only documents the Start Pos error, and no quit-crash stack trace has yet been supplied. Do not label or distribute this patch as a confirmed crash fix until it is built and tested in-game.

To complete diagnosis, capture the newest `Geometry Dash/geode/crashlogs/` crash report immediately after reproducing the quit crash, along with `geode/logs/latest.log` if possible.


## 1.0.4 crash report diagnosis (15 October 2026)

User crash stack:
`GeometryDash.exe!EditorUI::updateZoom` <- `lovestudios.part_prac.dll!PartPracSelector::init` <- `LevelEditorLayer::create` <- `pp::restoreSelector` <- `pp::SceneBoundary::update`.

When returning to the selector, `restoreSelector` calls `LevelEditorLayer::create`, which creates the UI as part of its own initialization. The old code called `updateZoom(session.cameraScale)` inside `EditorUI::init`, before the containing editor completed `LevelEditorLayer::init`; the report shows `updateZoom` reading a null pointer. This is consistent with premature access to a not-yet-complete native editor object. `v1.0.4` copies the saved camera parameters during `init`, then defers zoom restoration to an `EditorUI`-owned cocos scheduler callback after it is attached to the restored scene. Callback guards reject an editor/scene that no longer matches the Part Prac session. 

A separately delivered emergency v1.0.2 `.geode` was patched at the single confirmed Windows code offset to **skip** the problematic native call entirely; unlike v1.0.4 source, it cannot defer it without a proper compilation. Consequently that DLL preserves the segment and position, but resets zoom to the editor's default on return. It is an **experimental binary patch** based on the existing StartPos hotfix, NOT a build of this v1.0.4 source. Only its binary diff/package structure were verified, not its in-game execution.
