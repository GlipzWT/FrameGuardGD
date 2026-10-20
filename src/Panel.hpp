#pragma once
#include <Geode/ui/Popup.hpp>
#include <Geode/binding/Slider.hpp>
#include <map>
namespace fg {
class Panel : public geode::Popup {
    cocos2d::CCLabelBMFont* m_count=nullptr;
    cocos2d::CCLabelBMFont* m_time=nullptr;
    Slider* m_slider=nullptr;
    std::map<int,cocos2d::CCLabelBMFont*> m_values;
    static bool s_open;
    bool setup();
    void refresh();
    void setWindow(int);
    void onSlider(cocos2d::CCObject*);
    void onMinus(cocos2d::CCObject*);
    void onPlus(cocos2d::CCObject*);
    void onToggle(cocos2d::CCObject*);
    void onSettings(cocos2d::CCObject*);
    void onInfo(cocos2d::CCObject*);
    void onClose(cocos2d::CCObject*) override;
public:
    ~Panel() override;
    static void open();
};
}
