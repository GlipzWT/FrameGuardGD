#include "Panel.hpp"
#include "Assist.hpp"
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <cmath>
using namespace geode::prelude;
namespace fg {
bool Panel::s_open=false;
namespace {
char const* keys[]={"enabled","rollback","presses","releases","hud"};
ccColor3B const ink{218,231,242}, muted{134,157,180}, accent{69,225,196};
CCLabelBMFont* label(CCNode* parent,char const* text,float x,float y,float scale,
                      ccColor3B color=ink) {
    auto n=CCLabelBMFont::create(text,"bigFont.fnt");
    n->setScale(scale); n->setPosition({x,y}); n->setColor(color); parent->addChild(n);
    return n;
}
void card(CCNode* parent,float x,float y,float w,float h) {
    auto layer=CCLayerColor::create({21,31,47,230},w,h);
    layer->setPosition({x,y}); parent->addChild(layer,-1);
}
}
Panel::~Panel() { s_open=false; }
void Panel::open() {
    if(s_open) return;
    if(auto pl=PlayLayer::get()) {
        // Pausing flushes pending input / commits an unresolved grace death.
        Assist::get().suspend();
        if(!pl->m_isPaused) pl->pauseGame(false);
    }
    auto panel=new Panel;
    if(panel->init(382,302,"GJ_square01.png") && panel->setup()) {
        panel->autorelease(); s_open=true; panel->show();
    } else delete panel;
}
bool Panel::setup() {
    m_bgSprite->setColor({17,24,37});
    setTitle("FRAMEGUARD","bigFont.fnt",0.72f,23);
    label(m_mainLayer,"WAVE ASSIST  /  EXPERIMENTAL ALPHA",191,259,0.27f,accent);
    auto rule=CCLayerColor::create({69,225,196,255},330,2);
    rule->setPosition({26,247}); m_mainLayer->addChild(rule);
    card(m_mainLayer,22,163,338,73);
    auto caption=label(m_mainLayer,"CORRECTION WINDOW",105,218,0.32f,muted);
    (void)caption;
    m_count=label(m_mainLayer,"5 frames",296,218,0.47f,accent);
    m_slider=Slider::create(this,menu_selector(Panel::onSlider),0.82f);
    m_slider->setPosition({187,192}); m_mainLayer->addChild(m_slider);
    label(m_mainLayer,"0",46,191,0.3f,muted);
    label(m_mainLayer,"10",331,191,0.3f,muted);
    m_time=label(m_mainLayer,"Physics updates, not render FPS",191,174,0.24f,muted);
    m_buttonMenu->setPosition({0,0});
    // Restore Popup's standard close button position after moving its menu.
    m_closeBtn->setPosition({15,287});
    auto small=[&](char const* text,float x,SEL_MenuHandler cb) {
        auto l=CCLabelBMFont::create(text,"bigFont.fnt"); l->setScale(0.5f);
        auto b=CCMenuItemSpriteExtra::create(l,this,cb);
        b->setPosition({x,218}); m_buttonMenu->addChild(b);
    };
    small("-",230,menu_selector(Panel::onMinus));
    small("+",350,menu_selector(Panel::onPlus));
    card(m_mainLayer,22,49,338,106);
    char const* names[]={"Assist (Practice only)","Local rollback [EXPERIMENTAL]",
        "Correct presses","Correct releases","Show status HUD"};
    for(int i=0;i<5;++i) {
        float y=140-i*19;
        auto title=label(m_mainLayer,names[i],36,y,i==1?0.26f:0.30f,
                         i==1?ccColor3B{255,189,105}:ink);
        title->setAnchorPoint({0,0.5f});
        auto text=CCLabelBMFont::create("OFF","bigFont.fnt"); text->setScale(0.32f);
        m_values[i]=text;
        auto button=CCMenuItemSpriteExtra::create(text,this,menu_selector(Panel::onToggle));
        button->setTag(i); button->setPosition({331,y}); m_buttonMenu->addChild(button);
    }
    auto settings=ButtonSprite::create("Settings / binds"); settings->setScale(0.55f);
    auto settingsBtn=CCMenuItemSpriteExtra::create(settings,this,menu_selector(Panel::onSettings));
    settingsBtn->setPosition({119,28}); m_buttonMenu->addChild(settingsBtn);
    auto info=ButtonSprite::create("Limits"); info->setScale(0.55f);
    auto infoBtn=CCMenuItemSpriteExtra::create(info,this,menu_selector(Panel::onInfo));
    infoBtn->setPosition({284,28}); m_buttonMenu->addChild(infoBtn);
    refresh(); return true;
}
void Panel::refresh() {
    auto cfg=Config::read();
    auto text=std::to_string(cfg.window)+" frames"; m_count->setString(text.c_str());
    m_slider->setValue(cfg.window/10.0f);
    for(auto const& [i,l]:m_values) {
        bool on=Mod::get()->getSettingValue<bool>(keys[i]);
        l->setString(on?"ON":"OFF"); l->setColor(on?accent:muted);
    }
}
void Panel::setWindow(int n) {
    Mod::get()->setSettingValue<int64_t>("window",std::clamp(n,0,10));
    Assist::get().settingsChanged(); refresh();
}
void Panel::onSlider(CCObject*) { setWindow(static_cast<int>(std::lround(m_slider->getValue()*10))); }
void Panel::onMinus(CCObject*) { setWindow(Config::read().window-1); }
void Panel::onPlus(CCObject*) { setWindow(Config::read().window+1); }
void Panel::onToggle(CCObject* sender) {
    int i=sender->getTag(); if(i<0||i>4) return;
    auto m=Mod::get(); m->setSettingValue<bool>(keys[i],!m->getSettingValue<bool>(keys[i]));
    Assist::get().settingsChanged(); refresh();
}
void Panel::onSettings(CCObject*) { geode::openSettingsPopup(Mod::get()); }
void Panel::onInfo(CCObject*) {
    FLAlertLayer::create("Experimental alpha",
        "<cy>Practice Mode only.</c> Single forward Wave in static levels.\n\n"
        "Prediction uses <co>approximate AABB hitboxes</c>, not the full GD physics engine. "
        "It may choose a bad offset.\n\n"
        "Rollback restores <co>only player state</c>. Triggers, music and world time are not rewound. "
        "While awaiting late input it briefly suppresses collision death, at most the selected "
        "number of physics updates. No input means normal death.\n\n"
        "Do not use for records. See README_RU.md before enabling.","OK")->show();
}
void Panel::onClose(CCObject* sender) { s_open=false; Popup::onClose(sender); }
}
