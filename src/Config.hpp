#pragma once
#include <Geode/Geode.hpp>
#include <Geode/loader/SettingV3.hpp>
namespace fg {
struct Config {
    bool enabled=false, rollback=false, presses=true, releases=true, hud=true;
    int window=5;
    static Config read() {
        auto m=geode::Mod::get();
        return {m->getSettingValue<bool>("enabled"),
            m->getSettingValue<bool>("rollback"),
            m->getSettingValue<bool>("presses"),
            m->getSettingValue<bool>("releases"),
            m->getSettingValue<bool>("hud"),
            static_cast<int>(m->getSettingValue<int64_t>("window"))};
    }
};
}
