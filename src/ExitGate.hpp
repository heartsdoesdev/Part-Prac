#pragma once
#include <utility>

namespace partprac {
// Consume exit requests only after the native scene has resumed AND every object
// owned by the scene being exited has been released. This keeps teardown on the
// engine's native path, instead of replacing scenes inside a pause-menu callback.
class ExitGate {
public:
    enum class Action { None, RestoreSelector, Leave };
private:
    Action pending=Action::None;
    unsigned settledFrames=0;
public:
    void arm(Action action) { pending=action; settledFrames=0; }
    bool pendingAction() const { return pending!=Action::None; }
    Action advance(bool hostActive,bool oldLayerAlive,bool oldUIAlive) {
        if(pending==Action::None||!hostActive||oldLayerAlive||oldUIAlive) {
            settledFrames=0;
            return Action::None;
        }
        // One extra frame after destruction for deferred cocos and Geode cleanup.
        if(++settledFrames<2) return Action::None;
        return std::exchange(pending,Action::None);
    }
};
}
