# v1.0.4 — Exit crash fix
- Fix crash restoring the selector after gameplay: defer EditorUI::updateZoom
  until the rebuilt editor has entered its scene, instead of calling it during
  EditorUI::init / LevelEditorLayer::create.
- Check that the selector and its UI are still active before restoring zoom.
- Includes v1.0.3 safer native quit / relay and v1.0.2 Start Pos validation fix.

# v1.0.3 — Exit lifecycle source patch (experimental)
- Use native PlayLayer::onQuit cleanup instead of replacing scenes during the pause-menu callback.
- Defer editor recreation until native scene pop returns to the original level page.
- Use native pop for selector Leave; delay session cleanup until editor and UI are gone.
- Add ExitGate lifecycle tests, including delayed teardown and interrupted scene activity.
- **Not compiled for Windows / not tested in-game; original crash report still needed.**

# v1.0.2 verification hotfix (binary patch)

- Prevent valid native Start Pos markers from being rejected by strict coordinate matching.
- Keep Start Pos ID 31 and enabled-state validation. The built DLL still guards against launching at 0% when the game does not activate a native Start Pos.
- Not yet gameplay-tested. Windows rebuild from the supplied source is recommended for a durable release.

# v1.0.2

- Assign the generated Start Pos its required object ID 31 and selected coordinates.
- Validate the serialized marker before launch and refuse a silent full-level start if the game ignores it.
- Prac always jumps to the selection; Capture remains an explicit separate button.
- Add nine start-position regression checks.

# v1.0.1

- Fix editor-to-game loading crash with Tinker: fully release the old editor and UI before creating gameplay objects.
- Rebuild the selector on return and restore the selected range, pan and zoom.
- Wait for gameplay teardown before reconstructing the editor; leave through the same scene boundary.
- Add regression tests for retained UI/layer ownership and exactly-once transitions.

# v1.0.0

- Shift-click entry on downloaded and local level Play buttons.
- Read-only native-editor selector with pan, zoom, entry marker and section bounds.
- Native Start Pos looping, automatic death retry and music sync.
- Optional exact-entry checkpoint capture after a real lead-in.
- Whole-level data preserved, temporary copies and score-save protection.
