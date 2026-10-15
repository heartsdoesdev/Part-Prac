#include "../src/SceneGate.hpp"
#include <memory>
#include <cstdlib>
#include <iostream>
void check(bool value) { if(!value) std::abort(); }
struct EditorUI {
    bool& hooksEnabled;
    explicit EditorUI(bool& hooks):hooksEnabled(hooks) {}
    ~EditorUI() { hooksEnabled=false; }
};
struct Editor { std::shared_ptr<EditorUI> ui; };
int main() {
    bool hooksEnabled=true;
    auto ui=std::make_shared<EditorUI>(hooksEnabled);
    auto editor=std::make_shared<Editor>(Editor{ui});
    std::weak_ptr<Editor> oldEditor=editor;
    std::weak_ptr<EditorUI> oldUI=ui;
    int created=0;
    partprac::SceneGate gate([&] { check(!hooksEnabled); ++created; });
    check(!gate.advance(!oldEditor.expired(),!oldUI.expired()));
    editor.reset(); // The scene is gone, but another owner still retains the editor UI.
    check(!gate.advance(!oldEditor.expired(),!oldUI.expired()));
    check(created==0&&hooksEnabled);
    ui.reset(); // Editor-scoped hook cleanup finally runs.
    check(gate.advance(!oldEditor.expired(),!oldUI.expired()));
    check(created==1&&!hooksEnabled);
    check(!gate.advance(false,false));
    partprac::SceneGate reentrant;
    reentrant=partprac::SceneGate([&] { check(!reentrant.advance(false,false)); ++created; });
    check(reentrant.advance(false,false)); check(created==2);
    bool called=false;
    partprac::SceneGate stillOwned([&] { called=true; });
    check(!stillOwned.advance(true,false)); check(!called);
    std::cout<<"Scene teardown, retained UI, retained layer, one-shot and re-entry checks passed\n";
}
