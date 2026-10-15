"""Prevent native updateZoom from being called before LevelEditorLayer::create completes."""
from pathlib import Path
s=(Path(__file__).resolve().parent.parent/'src'/'main.cpp').read_text()
selector=s.split('class $modify(PartPracSelector,EditorUI) {', 1)[1]
init=selector.split('bool init(LevelEditorLayer* editor) {',1)[1].split('void zoom(float factor)',1)[0]
assert 'updateZoom(pp::session.cameraScale)' not in init
assert 'scheduleOnce(schedule_selector(PartPracSelector::restoreCameraWhenReady),0.f)' in init
scheduled=selector.split('void restoreCameraWhenReady(float) {',1)[1].split('bool init(LevelEditorLayer* editor)',1)[0]
assert 'updateZoom(std::clamp' in scheduled
assert 'pp::session.selector' in scheduled and 'pp::session.transitioning' in scheduled
print('Deferred camera restoration source regression test passed')
