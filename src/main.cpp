#include "Assist.hpp"
#include "Panel.hpp"
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/loader/GameEvent.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
using namespace geode::prelude;

class $modify(FGInput, GJBaseGameLayer) {
    void handleButton(bool down,int button,bool isPlayer1) {
        auto pl=PlayLayer::get();
        if(pl && static_cast<GJBaseGameLayer*>(pl)==this && isPlayer1 &&
           button==static_cast<int>(PlayerButton::Jump) && fg::Assist::get().input(pl,down))
            return;
        GJBaseGameLayer::handleButton(down,button,isPlayer1);
    }
};
class $modify(FGPlayer, PlayerObject) {
    void update(float dt) {
        fg::Assist::get().before(this,dt);
        PlayerObject::update(dt);
        fg::Assist::get().after(this,dt);
    }
};
class $modify(FGPlay, PlayLayer) {
    struct Fields { CCLabelBMFont* hud=nullptr; };
    bool init(GJGameLevel* level,bool replay,bool skipObjects) {
        if(!PlayLayer::init(level,replay,skipObjects)) return false;
        fg::Assist::get().reset(this);
        auto size=CCDirector::sharedDirector()->getWinSize();
        auto hud=CCLabelBMFont::create("FrameGuard alpha | Practice only","bigFont.fnt");
        hud->setScale(0.24f); hud->setAnchorPoint({0,1});
        hud->setPosition({8,size.height-8}); hud->setColor({69,225,196});
        hud->setOpacity(210); hud->setID("frameguard-status");
        addChild(hud,10000); m_fields->hud=hud;
        return true;
    }
    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        if(auto hud=m_fields->hud) {
            auto cfg=fg::Config::read(); hud->setVisible(cfg.hud && cfg.enabled);
            std::string text="FG ALPHA | "+fg::Assist::get().status();
            hud->setString(text.c_str()); hud->limitLabelWidth(440,0.24f,0.14f);
        }
    }
    void destroyPlayer(PlayerObject* p,GameObject* object) {
        if(fg::Assist::get().interceptDeath(this,p,object)) return;
        fg::Assist::get().normalDeath();
        PlayLayer::destroyPlayer(p,object);
    }
    void resetLevel() {
        fg::Assist::get().reset(this);
        PlayLayer::resetLevel();
        fg::Assist::get().reset(this);
    }
    void pauseGame(bool unfocused) {
        fg::Assist::get().suspend();
        PlayLayer::pauseGame(unfocused);
    }
    void togglePracticeMode(bool value) {
        fg::Assist::get().suspend();
        PlayLayer::togglePracticeMode(value);
        fg::Assist::get().reset(this);
    }
    void loadFromCheckpoint(CheckpointObject* object) {
        fg::Assist::get().reset(this);
        PlayLayer::loadFromCheckpoint(object);
        fg::Assist::get().reset(this);
    }
    void onExit() {
        fg::Assist::get().detach(this);
        PlayLayer::onExit();
    }
};
class $modify(FGMenu, MenuLayer) {
    bool init() {
        if(!MenuLayer::init()) return false;
        auto size=CCDirector::sharedDirector()->getWinSize();
        auto menu=CCMenu::create(); menu->setPosition({0,0});
        menu->setID("frameguard-menu");
        auto text=CCLabelBMFont::create("FG","bigFont.fnt");
        text->setScale(0.45f); text->setColor({69,225,196});
        auto btn=CCMenuItemSpriteExtra::create(text,this,menu_selector(FGMenu::onFrameGuard));
        btn->setPosition({size.width-30,size.height-24}); menu->addChild(btn);
        addChild(menu,100); return true;
    }
    void onFrameGuard(CCObject*) { fg::Panel::open(); }
};
$on_game(Loaded) {
    listenForKeybindSettingPresses("open-menu",[](Keybind const&,bool down,bool repeat,double) {
        if(down && !repeat) fg::Panel::open();
    });
    listenForKeybindSettingPresses("toggle-assist",[](Keybind const&,bool down,bool repeat,double) {
        if(!down || repeat) return;
        auto mod=Mod::get();
        mod->setSettingValue<bool>("enabled",!mod->getSettingValue<bool>("enabled"));
        fg::Assist::get().settingsChanged();
    });
}
