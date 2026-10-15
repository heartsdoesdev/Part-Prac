#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <Geode/Geode.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include <Geode/modify/LevelPage.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJGameLevel.hpp>
#include <Geode/ui/Popup.hpp>
#include <algorithm>
#include <functional>
#include "Range.hpp"
#include "SceneGate.hpp"
#include "ExitGate.hpp"

using namespace geode::prelude;
namespace pp {
class ExitRelay;
struct Session {
    Ref<GJGameLevel> source, editorLevel, playLevel;
    LevelEditorLayer* selector=nullptr; // Valid only while the selector scene is active.
    ExitRelay* exitRelay=nullptr; // Owned by the original, underlying scene.
    partprac::Range range;
    bool selecting=false, playing=false, exact=false, transitioning=false;
    CCPoint cameraPosition;
    float cameraScale=1.f;
    bool restoreCamera=false;
    int originalScene=0;
    std::string fullLevel;
    std::string launchError;
};
Session session;
bool held(int key) { return (GetAsyncKeyState(key)&0x8000)!=0; }
bool ownEditor() { return session.selecting&&session.selector; }
void alert(std::string message) { FLAlertLayer::create("Part Prac",message,"OK")->show(); }

GJGameLevel* copyLevel(GJGameLevel* source) {
    auto copy=GJGameLevel::create();
    copy->copyLevelInfo(source);
    copy->m_levelString=source->m_levelString;
    copy->m_levelName="Part Prac - "+source->m_levelName;
    copy->m_audioTrack=source->m_audioTrack;
    copy->m_songID=source->m_songID;
    copy->m_songIDs=source->m_songIDs; copy->m_sfxIDs=source->m_sfxIDs;
    copy->m_levelType=GJLevelType::Editor;
    copy->setLevelID(0); copy->setOriginalLevel(0);
    copy->setStars(0); copy->setNormalPercent(0);
    return copy;
}

// Replacing a scene is deferred by cocos. Wait for both layer and UI destruction,
// rather than clearing global pointers while editor-scoped hooks are still installed.
class SceneBoundary : public CCLayer {
    WeakRef<CCNode> oldLayer;
    WeakRef<CCNode> oldUI;
    partprac::SceneGate gate;
public:
    static CCScene* scene(CCNode* layer,CCNode* ui,std::function<void()> callback) {
        auto boundary=new SceneBoundary;
        if(!boundary->init()) { delete boundary; return nullptr; }
        boundary->autorelease(); boundary->oldLayer=layer; boundary->oldUI=ui;
        boundary->gate=partprac::SceneGate(std::move(callback));
        auto size=CCDirector::sharedDirector()->getWinSize();
        auto text=CCLabelBMFont::create("Part Prac | Loading...","bigFont.fnt");
        text->setScale(.5f); text->setPosition({size.width/2,size.height/2});
        boundary->addChild(text);
        auto scene=CCScene::create(); scene->addChild(boundary); return scene;
    }
    void onEnter() override { CCLayer::onEnter(); scheduleUpdate(); }
    void update(float) override {
        // No new gameplay objects until editor-owned hooks have had their destructors run.
        // Evaluate independently: checking a weak reference can release its last owner.
        bool layerAlive=oldLayer.valid();
        bool uiAlive=oldUI.valid();
        if(gate.advance(layerAlive,uiAlive)) unscheduleUpdate();
    }
};

// Native quitting pops the practice scene. An inert relay on the underlying
// level page waits for normal teardown to finish before pushing the editor again.
void restoreSelector(bool pushFromOriginal=false);
void completeLeave();
class ExitRelay : public CCNode {
    WeakRef<CCNode> oldLayer;
    WeakRef<CCNode> oldUI;
    partprac::ExitGate gate;
public:
    static ExitRelay* create() {
        auto node=new ExitRelay;
        if(node&&node->init()) { node->autorelease(); return node; }
        delete node; return nullptr;
    }
    void arm(CCNode* layer,CCNode* ui,partprac::ExitGate::Action action) {
        oldLayer=layer; oldUI=ui;
        gate.arm(action); scheduleUpdate();
    }
    void update(float) override {
        auto director=CCDirector::sharedDirector();
        // Calculate each weak reference separately: Geode may release the last
        // owner while checking it. Never create a layer while one is still alive.
        bool layerAlive=oldLayer.valid(), uiAlive=oldUI.valid();
        auto action=gate.advance(director->getRunningScene()==getParent(),layerAlive,uiAlive);
        if(action==partprac::ExitGate::Action::None) return;
        unscheduleUpdate();
        if(action==partprac::ExitGate::Action::RestoreSelector) restoreSelector(true);
        else {
            // completeLeave detaches this node. Keep it alive during update().
            Ref<ExitRelay> keep=this;
            completeLeave();
        }
    }
};

void completeLeave() {
    auto gm=GameManager::sharedState();
    gm->m_levelEditorLayer=nullptr; gm->m_gameLayer=nullptr;
    gm->m_sceneEnum=session.originalScene;
    if(auto relay=session.exitRelay) relay->removeFromParentAndCleanup(true);
    session={};
    gm->fadeInMenuMusic();
}

void restoreSelector(bool pushFromOriginal) {
    session.playing=false; session.selecting=true;
    auto gm=GameManager::sharedState();
    gm->m_playLayer=nullptr; gm->m_gameLayer=nullptr;
    auto editor=LevelEditorLayer::create(session.editorLevel,false);
    if(!editor) {
        session.transitioning=false; session.selecting=false;
        gm->m_sceneEnum=session.originalScene;
        if(!pushFromOriginal) CCDirector::sharedDirector()->popScene();
        completeLeave();
        alert("Could not reopen the selector. Please enter Part Prac again."); return;
    }
    session.selector=editor; session.transitioning=false;
    auto scene=CCScene::create(); scene->addChild(editor);
    if(pushFromOriginal) CCDirector::sharedDirector()->pushScene(scene);
    else CCDirector::sharedDirector()->replaceScene(scene);
    if(!session.launchError.empty()) {
        auto message=std::exchange(session.launchError,{});
        Loader::get()->queueInMainThread([message] { alert(message); });
    }
}

void openSelector(GJGameLevel* source) {
    if(session.selecting||session.playing||session.transitioning) return;
    if(source->m_levelString.empty()) {
        alert("Download the level first, then <cg>Shift + click Play</c> again."); return;
    }
    if(source->isPlatformer()) {
        alert("This version selects a <cg>forward-moving classic level</c> section. Platformer routes are not supported yet."); return;
    }
    session={}; session.source=source;
    session.originalScene=GameManager::sharedState()->m_sceneEnum;
    session.editorLevel=copyLevel(source);
    session.selecting=true;
    // Do not register this private copy with LocalLevelManager or save it to disk.
    auto editor=LevelEditorLayer::create(session.editorLevel,false);
    if(!editor) { session={}; alert("This level could not be opened in the selector."); return; }
    session.selector=editor;
    // The host stays on the director's scene stack until Part Prac leaves.
    auto host=CCDirector::sharedDirector()->getRunningScene();
    auto relay=ExitRelay::create();
    if(!host||!relay) { session={}; alert("Could not prepare the Part Prac return path."); return; }
    host->addChild(relay); session.exitRelay=relay;
    session.fullLevel=partprac::withoutStartPositions(std::string(editor->getLevelString()));
    auto scene=CCScene::create(); scene->addChild(editor);
    CCDirector::sharedDirector()->pushScene(scene);
}

void ask(GJGameLevel* level) {
    Ref<GJGameLevel> keep=level;
    createQuickPopup("Enter Part Prac Mode",
        "Select a section and practise it on repeat.\n<cg>Your original level stays unchanged.</c>",
        "Cancel","Enter",[keep](FLAlertLayer*,bool yes) { if(yes) openSelector(keep); });
}

void closeSelector() {
    if(session.transitioning||!session.selector) return;
    auto relay=session.exitRelay;
    if(!relay) { alert("Could not safely leave Part Prac. Please retry."); return; }
    relay->arm(session.selector,session.selector->m_editorUI,partprac::ExitGate::Action::Leave);
    FMODAudioEngine::sharedEngine()->stopAllMusic(true);
    session.selecting=false; session.transitioning=true; session.selector=nullptr;
    // The selector was pushed onto the original level-page scene; let cocos
    // pop it normally, then clean up when the underlying scene resumes.
    CCDirector::sharedDirector()->popScene();
}

// Infer ordinary portals for instant starts. Route-dependent portal states require Exact capture.
void inferSettings(LevelSettingsObject* settings,LevelEditorLayer* editor,float x) {
    std::vector<GameObject*> objects;
    for(auto object:CCArrayExt<GameObject*>(editor->m_objects))
        if(object->getPositionX()<x) objects.push_back(object);
    std::stable_sort(objects.begin(),objects.end(),[](auto a,auto b){return a->getPositionX()<b->getPositionX();});
    for(auto o:objects) {
        switch(o->m_objectID) {
            case 12: settings->m_startMode=0; break;
            case 13: settings->m_startMode=1; break;
            case 47: settings->m_startMode=2; break;
            case 111: settings->m_startMode=3; break;
            case 660: settings->m_startMode=4; break;
            case 745: settings->m_startMode=5; break;
            case 1331: settings->m_startMode=6; break;
            case 1933: settings->m_startMode=7; break;
            case 200: settings->m_startSpeed=Speed::Slow; break;
            case 201: settings->m_startSpeed=Speed::Normal; break;
            case 202: settings->m_startSpeed=Speed::Fast; break;
            case 203: settings->m_startSpeed=Speed::Faster; break;
            case 1334: settings->m_startSpeed=Speed::Fastest; break;
            case 99: settings->m_startMini=false; break;
            case 101: settings->m_startMini=true; break;
            case 286: settings->m_startDual=true; break;
            case 287: settings->m_startDual=false; break;
            case 10: settings->m_isFlipped=true; break;
            case 11: settings->m_isFlipped=false; break;
        }
    }
}

void launch(bool exact) {
    if(session.transitioning||session.playing) return;
    if(!session.range.valid()) { alert("Drag <cg>left to right</c> across at least one block. Start your drag at the player's desired entry position, above the floor."); return; }
    auto editor=session.selector;
    if(!editor||!editor->m_levelSettings) { alert("Please reopen the level selector."); return; }
    auto level=copyLevel(session.source);
    std::string data=session.fullLevel;
    if(!exact) {
        auto start=StartPosObject::create();
        auto settings=LevelSettingsObject::objectFromString(editor->m_levelSettings->getSaveString());
        if(!start||!settings) { alert("Could not prepare the starting position."); return; }
        inferSettings(settings,editor,session.range.startX);
        settings->m_startsWithStartPos=true;
        settings->m_disableStartPos=false;
        settings->m_reverseGameplay=false;
        start->setSettings(settings);
        // create() initializes the subclass, but the game object's factory normally
        // assigns its numeric ID. Without 31 the serialized marker is not a Start Pos.
        start->m_objectID=31;
        start->m_startPosition=CCPoint{session.range.startX,session.range.startY};
        start->setPosition(start->m_startPosition);
        auto encoded=std::string(start->getSaveString(editor));
        if(!partprac::validStartPosition(encoded,session.range)) {
            log::error("Start Pos serialization failed validation: {}",encoded);
            alert("Could not create the selected start position. The level was not started."); return;
        }
        data+=encoded;
        if(data.back()!=';') data+=';';
    }
    level->m_levelString=data;
    session.playLevel=level; session.exact=exact;
    session.cameraPosition=editor->m_objectLayer->getPosition();
    session.cameraScale=editor->m_objectLayer->getScale(); session.restoreCamera=true;
    session.transitioning=true; session.selecting=false;
    FMODAudioEngine::sharedEngine()->stopAllMusic(true);
    auto bridge=SceneBoundary::scene(editor,editor->m_editorUI,[] {
        // All old editor/UI destructors have now run (including Tinker's hook cleanup).
        auto gm=GameManager::sharedState();
        gm->m_levelEditorLayer=nullptr; gm->m_gameLayer=nullptr;
        session.selector=nullptr; session.playing=true;
        auto play=PlayLayer::create(session.playLevel,false,false);
        if(!play) {
            session.playing=false; gm->m_playLayer=nullptr;
            restoreSelector(); alert("Could not start this section."); return;
        }
        session.transitioning=false;
        auto scene=CCScene::create(); scene->addChild(play);
        CCDirector::sharedDirector()->replaceScene(scene);
    });
    if(!bridge) { session.transitioning=false; session.selecting=true; alert("Could not prepare the scene transition."); return; }
    session.selector=nullptr;
    CCDirector::sharedDirector()->replaceScene(bridge);
}

class PracticeMenu : public Popup {
    bool initMenu() {
        if(!Popup::init(380,230)) return false;
        setTitle("Part Prac | Love Studios");
        auto text=CCLabelBMFont::create(session.range.valid()
            ? "Prac: jump straight to your section.\nCapture: play the lead-in once to record\ninput-dependent triggers and the actual entry."
            : "Drag from your entry point to the end.\nSpace + drag pans. +/- buttons zoom.\nChoose a safe starting point above the floor.","bigFont.fnt");
        text->setScale(.36f); text->setPosition({190,151}); m_mainLayer->addChild(text);
        auto menu=CCMenu::create(); menu->setPosition({0,0}); m_mainLayer->addChild(menu);
        auto add=[&](char const* label,SEL_MenuHandler callback,float x,float y) {
            auto item=CCMenuItemSpriteExtra::create(ButtonSprite::create(label),this,callback);
            item->setScale(.75f); item->setPosition({x,y}); menu->addChild(item);
        };
        add("Prac",menu_selector(PracticeMenu::onPrac),110,90);
        add("Capture",menu_selector(PracticeMenu::onCapture),270,90);
        add("Select",menu_selector(PracticeMenu::onSelect),110,38);
        add("Leave",menu_selector(PracticeMenu::onLeave),270,38);
        return true;
    }
    void onPrac(CCObject*) { if(!session.range.valid()) { alert("Select a valid section first."); return; } onClose(nullptr); launch(false); }
    void onCapture(CCObject*) { if(!session.range.valid()) { alert("Select a valid section first."); return; } onClose(nullptr); launch(true); }
    void onSelect(CCObject*) { onClose(nullptr); }
    void onLeave(CCObject*) { onClose(nullptr); closeSelector(); }
public:
    static PracticeMenu* create() { auto p=new PracticeMenu; if(p->initMenu()) { p->autorelease(); return p; } delete p; return nullptr; }
};
}

class $modify(PartPracLevelInfo,LevelInfoLayer) {
    void onPlay(CCObject* sender) {
        if(pp::held(VK_SHIFT)) { pp::ask(m_level); return; }
        LevelInfoLayer::onPlay(sender);
    }
};
class $modify(PartPracLocalLevel,EditLevelLayer) {
    void onPlay(CCObject* sender) {
        if(pp::held(VK_SHIFT)) { pp::ask(m_level); return; }
        EditLevelLayer::onPlay(sender);
    }
};
class $modify(PartPracMainLevel,LevelPage) {
    void onPlay(CCObject* sender) {
        if(pp::held(VK_SHIFT)&&m_level) {
            auto level=GameLevelManager::sharedState()->getMainLevel(m_level->m_levelID.value(),false);
            if(level) pp::ask(level);
            else pp::alert("Could not load this main level.");
            return;
        }
        LevelPage::onPlay(sender);
    }
};

class $modify(PartPracSelector,EditorUI) {
    struct Fields {
        bool ours=false, dragging=false, panning=false;
        CCPoint first, last;
        CCDrawNode* markers=nullptr;
        CCLabelBMFont* status=nullptr;
        bool cameraRestorePending=false;
        CCPoint cameraRestorePosition;
        float cameraRestoreScale=1.f;
    };
    // LevelEditorLayer::create() is still in progress when EditorUI::init()
    // returns. BetterEdit/Tinker/Good Grid hook updateZoom, and the native
    // method may dereference uninitialised editor data if called here.
    // A scheduled callback runs after the selector enters its scene.
    void restoreCameraWhenReady(float) {
        if (!m_fields->ours || !m_fields->cameraRestorePending) return;
        m_fields->cameraRestorePending=false;
        auto editor=m_editorLayer;
        if (!editor || !pp::session.selecting || pp::session.transitioning ||
            editor!=pp::session.selector ||
            editor->m_level!=pp::session.editorLevel.data() ||
            editor->m_editorUI!=this || !editor->m_objectLayer) return;
        updateZoom(std::clamp(m_fields->cameraRestoreScale,.1f,3.f));
        editor->m_objectLayer->setPosition(m_fields->cameraRestorePosition);
        pp::session.restoreCamera=false;
    }
    bool init(LevelEditorLayer* editor) {
        if(!EditorUI::init(editor)) return false;
        if(!pp::session.selecting||editor->m_level!=pp::session.editorLevel.data()) return true;
        m_fields->ours=true;
        // This is a read-only selector: hide build/edit controls, intercept edits and keep native rendering.
        for(auto child:CCArrayExt<CCNode*>(getChildren())) child->setVisible(false);
        auto size=CCDirector::sharedDirector()->getWinSize();
        auto backdrop=CCLayerColor::create({20,13,27,225},size.width,44);
        backdrop->setPosition({0,size.height-44}); addChild(backdrop,100);
        auto title=CCLabelBMFont::create("PART PRAC  /  LOVE STUDIOS","goldFont.fnt");
        title->setScale(.48f); title->setPosition({size.width/2,size.height-12}); addChild(title,101);
        auto help=CCLabelBMFont::create("Drag entry > end | Space+drag: pan | +/-: zoom | Esc: Prac","bigFont.fnt");
        help->setScale(.29f); help->setPosition({size.width/2,size.height-31}); addChild(help,101);
        m_fields->status=CCLabelBMFont::create("Choose an entry point above the floor","bigFont.fnt");
        m_fields->status->setScale(.33f); m_fields->status->setPosition({size.width/2,16}); addChild(m_fields->status,101);
        auto menu=CCMenu::create(); menu->setPosition({0,0}); addChild(menu,102);
        auto add=[&](char const* title,SEL_MenuHandler action,float x) {
            auto b=CCMenuItemSpriteExtra::create(ButtonSprite::create(title),this,action);
            b->setScale(.5f); b->setPosition({x,size.height-62}); menu->addChild(b);
        };
        add("+",menu_selector(PartPracSelector::zoomIn),size.width-30);
        add("-",menu_selector(PartPracSelector::zoomOut),size.width-65);
        add("Menu",menu_selector(PartPracSelector::openMenu),40);
        m_fields->markers=CCDrawNode::create(); editor->m_objectLayer->addChild(m_fields->markers,10000);
        if(pp::session.restoreCamera) {
            m_fields->cameraRestorePending=true;
            m_fields->cameraRestoreScale=pp::session.cameraScale;
            m_fields->cameraRestorePosition=pp::session.cameraPosition;
            // Never call EditorUI::updateZoom during EditorUI::init!
            scheduleOnce(schedule_selector(PartPracSelector::restoreCameraWhenReady),0.f);
        }
        if(pp::session.range.valid()) {
            auto range=pp::session.range;
            m_fields->first=CCPoint{range.startX,range.startY}; redraw({range.endX,range.endY});
        }
        return true;
    }
    void zoom(float factor) {
        auto layer=m_editorLayer->m_objectLayer;
        auto centre=CCDirector::sharedDirector()->getWinSize()/2;
        CCPoint point={centre.width,centre.height};
        auto local=layer->convertToNodeSpace(point);
        updateZoom(std::clamp(layer->getScale()*factor,.1f,3.f));
        auto moved=layer->convertToWorldSpace(local);
        layer->setPosition(layer->getPosition()+point-moved);
    }
    void zoomIn(CCObject*) { zoom(1.25f); }
    void zoomOut(CCObject*) { zoom(.8f); }
    void openMenu(CCObject*) { if(auto p=pp::PracticeMenu::create()) p->show(); }
    void redraw(CCPoint current) {
        auto a=m_fields->first;
        pp::session.range={a.x,a.y,current.x,current.y};
        auto draw=m_fields->markers; draw->clear();
        float bottom=std::min(a.y,current.y)-15,top=std::max(a.y,current.y)+15;
        CCPoint corners[]={{a.x,bottom},{current.x,bottom},{current.x,top},{a.x,top}};
        draw->drawPolygon(corners,4,{1.f,.25f,.65f,.13f},1.5f,{1.f,.35f,.7f,.95f});
        draw->drawDot(a,6,{.3f,1.f,.7f,1});
        draw->drawSegment({a.x,bottom},{a.x,top},1.5f,{.3f,1.f,.7f,1});
        auto text=fmt::format("Entry ({:.0f}, {:.0f})  >  End {:.0f}  |  {}",a.x,a.y,current.x,
            pp::session.range.valid()?"Esc > Prac":"Drag further to the right");
        m_fields->status->setString(text.c_str());
    }
    bool ccTouchBegan(CCTouch* touch,CCEvent* event) {
        if(!m_fields->ours) return EditorUI::ccTouchBegan(touch,event);
        auto p=touch->getLocation();
        if(p.y>CCDirector::sharedDirector()->getWinSize().height-80) return false;
        m_fields->dragging=true; m_fields->panning=pp::held(VK_SPACE); m_fields->last=p;
        m_fields->first=m_editorLayer->m_objectLayer->convertToNodeSpace(p);
        if(!m_fields->panning) redraw(m_fields->first);
        return true;
    }
    void ccTouchMoved(CCTouch* touch,CCEvent* event) {
        if(!m_fields->ours) { EditorUI::ccTouchMoved(touch,event); return; }
        if(!m_fields->dragging) return;
        auto p=touch->getLocation(); auto layer=m_editorLayer->m_objectLayer;
        if(m_fields->panning) layer->setPosition(layer->getPosition()+p-m_fields->last);
        else redraw(layer->convertToNodeSpace(p));
        m_fields->last=p;
    }
    void ccTouchEnded(CCTouch* touch,CCEvent* event) {
        if(!m_fields->ours) { EditorUI::ccTouchEnded(touch,event); return; }
        ccTouchMoved(touch,event); m_fields->dragging=false;
    }
    void ccTouchCancelled(CCTouch* touch,CCEvent* event) {
        if(!m_fields->ours) EditorUI::ccTouchCancelled(touch,event);
        else m_fields->dragging=false;
    }
    void keyBackClicked() {
        if(m_fields->ours) { openMenu(nullptr); return; }
        EditorUI::keyBackClicked();
    }
    void keyDown(enumKeyCodes key,double timestamp) {
        if(!m_fields->ours) { EditorUI::keyDown(key,timestamp); return; }
        if(key==enumKeyCodes::KEY_Escape) keyBackClicked();
    }
};

class $modify(PartPracPlay,PlayLayer) {
    struct Fields {
        bool ours=false, exact=false, started=false, queued=false, captured=false, leaving=false, deathCounted=false, startRejected=false;
        unsigned loops=0, deaths=0;
        Ref<CheckpointObject> checkpoint;
        CCLabelBMFont* status=nullptr;
    };
    bool init(GJGameLevel* level,bool replay,bool noObjects) {
        m_fields->ours=pp::session.playing&&level==pp::session.playLevel.data();
        m_fields->exact=pp::session.exact;
        if(!PlayLayer::init(level,replay,noObjects)) return false;
        if(!m_fields->ours) return true;
        m_isTestMode=true; m_practiceMusicSync=true;
        auto size=CCDirector::sharedDirector()->getWinSize();
        auto label=CCLabelBMFont::create("Part Prac","bigFont.fnt");
        label->setScale(.32f); label->setPosition({size.width/2,size.height-16});
        addChild(label,10000); m_fields->status=label; updateLabel();
        return true;
    }
    void updateLabel() {
        if(!m_fields->status) return;
        auto text=m_fields->exact&&!m_fields->captured
            ? "CAPTURE: play to the selected entry | No auto checkpoints"
            : fmt::format("PART PRAC  |  Loops {}  |  Deaths {}",m_fields->loops,m_fields->deaths);
        m_fields->status->setString(text.c_str());
    }
    void startGame() {
        if(m_fields->ours&&!m_fields->exact&&!m_startPosObject) {
            if(!m_fields->startRejected) {
                m_fields->startRejected=true;
                log::error("Part Prac: game did not activate the selected Start Pos; refusing a full-level start.");
                pp::session.launchError="The game did not activate the selected start position. Returned to the selector instead of starting the full level.";
                Ref<PartPracPlay> keep=this;
                Loader::get()->queueInMainThread([keep] {
                    if(PlayLayer::get()==keep.data()&&!keep->m_fields->leaving) keep->onQuit();
                });
            }
            return;
        }
        PlayLayer::startGame();
        if(!m_fields->ours||m_fields->started) return;
        m_fields->started=true;
        if(m_fields->exact) { PlayLayer::togglePracticeMode(true); m_practiceMusicSync=true; }
        m_isTestMode=true;
    }
    void startMusic() {
        if(m_fields->ours) m_practiceMusicSync=true;
        PlayLayer::startMusic();
    }
    void processCheckpoints() {
        if(!m_fields->ours) PlayLayer::processCheckpoints();
    }
    CheckpointObject* markCheckpoint() {
        if(m_fields->ours) return nullptr; // The selected entry is the only permitted checkpoint.
        return PlayLayer::markCheckpoint();
    }
    void removeCheckpoint(bool first) {
        if(!m_fields->ours) PlayLayer::removeCheckpoint(first);
    }
    void togglePracticeMode(bool enabled) {
        if(!m_fields->ours) { PlayLayer::togglePracticeMode(enabled); return; }
        // The pause menu cannot switch out of Part Prac's chosen reset behaviour.
        PlayLayer::togglePracticeMode(m_fields->exact); m_practiceMusicSync=true; m_isTestMode=true;
    }
    void requestLoop() {
        if(m_fields->queued||m_fields->leaving) return;
        m_fields->queued=true;
        Ref<PartPracPlay> keep=this;
        Loader::get()->queueInMainThread([keep] {
            if(!keep->m_fields->leaving&&PlayLayer::get()==keep.data()) {
                ++keep->m_fields->loops;
                keep->resetLevel(); keep->updateLabel();
            }
            keep->m_fields->queued=false;
        });
    }
    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        if(!m_fields->ours||!m_fields->started||m_fields->leaving||m_isPaused||!m_player1||m_player1->m_isDead) return;
        float x=m_player1->getPositionX();
        if(m_fields->exact&&!m_fields->captured&&x>=pp::session.range.startX) {
            auto checkpoint=PlayLayer::createCheckpoint();
            if(checkpoint) {
                PlayLayer::removeAllCheckpoints(); PlayLayer::storeCheckpoint(checkpoint);
                m_fields->checkpoint=checkpoint; m_fields->captured=true; updateLabel();
            }
        }
        if(x>=pp::session.range.endX&&(!m_fields->exact||m_fields->captured)) requestLoop();
    }
    void checkForEnd() {
        if(!m_fields->ours) { PlayLayer::checkForEnd(); return; }
        if(m_player1&&!m_player1->m_isDead&&m_player1->getPositionX()>=pp::session.range.endX) requestLoop();
    }
    void levelComplete() {
        if(m_fields->ours) { requestLoop(); return; }
        PlayLayer::levelComplete();
    }
    void resetLevel() {
        if(m_fields->ours) {
            unschedule(schedule_selector(PartPracPlay::retryDeath));
            m_fields->deathCounted=false;
            m_isTestMode=true; m_practiceMusicSync=true;
            if(m_fields->checkpoint) {
                PlayLayer::removeAllCheckpoints();
                PlayLayer::storeCheckpoint(m_fields->checkpoint);
            }
        }
        PlayLayer::resetLevel();
        if(m_fields->ours) { m_isTestMode=true; updateLabel(); }
    }
    void destroyPlayer(PlayerObject* player,GameObject* object) {
        bool alive=player&&!player->m_isDead;
        PlayLayer::destroyPlayer(player,object);
        if(m_fields->ours&&alive&&player->m_isDead&&!m_fields->deathCounted) {
            m_fields->deathCounted=true; ++m_fields->deaths; updateLabel();
            // Native auto-retry normally wins. This also retries when that user setting is off.
            scheduleOnce(schedule_selector(PartPracPlay::retryDeath),.65f);
        }
    }
    void retryDeath(float) {
        if(!m_fields->leaving&&!m_isPaused&&m_player1&&m_player1->m_isDead) resetLevel();
    }
    void onQuit() {
        if(!m_fields->ours) { PlayLayer::onQuit(); return; }
        if(m_fields->leaving||pp::session.transitioning) return;
        m_fields->leaving=true;
        unschedule(schedule_selector(PartPracPlay::retryDeath));
        auto relay=pp::session.exitRelay;
        if(relay)
            relay->arm(this,nullptr,partprac::ExitGate::Action::RestoreSelector);
        pp::session.playing=false; pp::session.transitioning=true;
        // CRITICAL: Never replace the scene directly from the pause menu's
        // onQuit callback. The base method performs GD's normal pause, gameplay,
        // singleton and audio cleanup and pops back to the underlying level page.
        PlayLayer::onQuit();
        if(!relay) {
            // Safe degradation: return to the level page rather than crash.
            pp::session={};
        }
    }

};

class $modify(PartPracNoScores,GJGameLevel) {
    void savePercentage(int percent,bool practice,int clicks,int attempts,bool valid) {
        if(this==pp::session.playLevel.data()||this==pp::session.editorLevel.data()) return;
        GJGameLevel::savePercentage(percent,practice,clicks,attempts,valid);
    }
};
