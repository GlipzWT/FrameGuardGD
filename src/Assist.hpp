#pragma once
#include "Config.hpp"
#include <frameguard/Planner.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/binding/PlayerObject.hpp>
#include <Geode/binding/PlayerCheckpoint.hpp>
#include <deque>
#include <cstdint>
#include <optional>
#include <string>
namespace fg {
class Assist {
    struct Entry {
        uint64_t tick;
        frameguard::State state;
        geode::Ref<PlayerCheckpoint> checkpoint;
    };
    struct Pending { uint64_t due; bool down; int offset; };
    PlayLayer* m_owner=nullptr; // reset before ownership changes; never owning
    std::deque<Entry> m_history;
    std::optional<Pending> m_pending;
    frameguard::Grace m_grace;
    geode::Ref<GameObject> m_deathObject;
    uint64_t m_tick=0, m_collisionAnchor=0;
    double m_dx=0, m_stepDt=0;
    cocos2d::CCPoint m_before;
    bool m_inverted=false;
    float m_size=0, m_speed=0;
    bool m_worldAudited=false, m_worldSupported=false;
    bool m_bypassDeath=false;
    std::string m_status="Disabled";
    Config m_last;
    bool m_haveConfig=false;
    void apply(PlayerObject* player,bool down);
    void flush(PlayerObject* player);
    void clearTemporal();
    bool compatible(PlayLayer*,PlayerObject*,Config const&);
    bool auditWorld(PlayLayer*);
    frameguard::State state(PlayerObject*);
    std::optional<frameguard::World> world(PlayLayer*,frameguard::State const&,int);
    void finishDeath(PlayLayer*);
public:
    static Assist& get();
    void bind(PlayLayer*);
    void reset(PlayLayer*);
    void detach(PlayLayer*);
    bool input(PlayLayer*,bool); // true = edge consumed / delivered directly
    void before(PlayerObject*,float);
    void after(PlayerObject*,float);
    bool interceptDeath(PlayLayer*,PlayerObject*,GameObject*);
    void settingsChanged();
    void normalDeath();
    void suspend();
    std::string const& status() const { return m_status; }
};
}
