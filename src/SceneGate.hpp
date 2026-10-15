#pragma once
#include <functional>
#include <utility>

namespace partprac {
// Ownership stays with the engine. The callback must never capture the old layer.
class SceneGate {
    std::function<void()> next;
public:
    explicit SceneGate(std::function<void()> callback = {}) : next(std::move(callback)) {}
    bool advance(bool layerAlive, bool uiAlive) {
        if(layerAlive || uiAlive || !next) return false;
        auto callback=std::move(next);
        next=nullptr; // Clear before invoking: callbacks may re-enter the scheduler.
        callback();
        return true;
    }
};
}
