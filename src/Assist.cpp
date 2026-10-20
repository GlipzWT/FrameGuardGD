#include "Assist.hpp"
#include <Geode/binding/GameObject.hpp>
#include <algorithm>
#include <cmath>
using namespace geode::prelude;
using namespace frameguard;
namespace fg {
Assist& Assist::get() { static auto* s=new Assist; return *s; }
void Assist::clearTemporal() {
    m_history.clear(); m_pending.reset(); m_grace.clear(); m_deathObject=nullptr;
}
void Assist::bind(PlayLayer* p) {
    if(m_owner==p) return;
    m_owner=p; clearTemporal(); m_tick=0; m_dx=0; m_stepDt=0;
    m_worldAudited=false; m_haveConfig=false; m_status="Warming up";
}
void Assist::reset(PlayLayer* p) {
    m_owner=nullptr; bind(p);
}
void Assist::detach(PlayLayer* p) {
    if(m_owner!=p) return;
    clearTemporal(); m_owner=nullptr; m_dx=0; m_worldAudited=false;
}
void Assist::apply(PlayerObject* p,bool down) {
    if(down) p->pushButton(PlayerButton::Jump);
    else p->releaseButton(PlayerButton::Jump);
}
void Assist::flush(PlayerObject* p) {
    if(m_pending) { auto v=m_pending->down; m_pending.reset(); apply(p,v); }
    m_history.clear();
}
void Assist::finishDeath(PlayLayer* pl) {
    if(!m_grace.active) return;
    auto object=m_deathObject; // keep object alive while original destruction runs
    m_grace.clear(); m_deathObject=nullptr; m_history.clear(); m_pending.reset();
    m_bypassDeath=true;
    pl->destroyPlayer(pl->m_player1,object.data());
    m_bypassDeath=false;
    m_status="No correction: normal death";
}
void Assist::suspend() {
    if(!m_owner) return;
    finishDeath(m_owner);
    if(m_owner->m_player1) flush(m_owner->m_player1);
    m_history.clear(); m_dx=0;
}
void Assist::normalDeath() {
    clearTemporal(); m_dx=0; m_status="Normal death";
}
void Assist::settingsChanged() {
    suspend(); m_haveConfig=false;
}
bool Assist::auditWorld(PlayLayer* pl) {
    if(!pl->m_objects) return false;
    CCObject* raw=nullptr;
    CCARRAY_FOREACH(pl->m_objects,raw) {
        auto o=static_cast<GameObject*>(raw);
        // Deliberately strict: even remote spawn/move/follow triggers make
        // local player-only rewind unsafe. No "guessing" about trigger scope.
        if(o->m_isTrigger || o->m_hasGroupParent || o->m_hasAreaParent ||
           o->m_hasExtendedCollision) return false;
        switch(o->m_objectType) {
            case GameObjectType::Solid:
            case GameObjectType::Hazard:
            case GameObjectType::Decoration:
            case GameObjectType::WavePortal:
            case GameObjectType::CubePortal:
            case GameObjectType::ShipPortal:
            case GameObjectType::BallPortal:
            case GameObjectType::UfoPortal:
            case GameObjectType::RobotPortal:
            case GameObjectType::SpiderPortal:
            case GameObjectType::SwingPortal:
            case GameObjectType::RegularSizePortal:
            case GameObjectType::MiniSizePortal:
            case GameObjectType::InverseGravityPortal:
            case GameObjectType::NormalGravityPortal:
                break;
            default: return false;
        }
    }
    return true;
}
bool Assist::compatible(PlayLayer* pl,PlayerObject* p,Config const& cfg) {
    if(!cfg.enabled || cfg.window<=0) { m_status="Disabled / 0 frames"; return false; }
    if(!pl->m_isPracticeMode) { m_status="Practice Mode required"; return false; }
    if(pl->m_isPaused || pl->m_hasCompletedLevel || p->m_isDead) return false;
    if(pl->m_isPlatformer || pl->m_gameState.m_isDualMode ||
       !p->m_isDart || p->m_isSideways || p->m_isGoingLeft ||
       p->m_isDashing || p->m_isLocked || p->m_controlsDisabled ||
       p->m_isOnSlope || p->m_isOnGround) {
        m_status="Unsupported player state"; return false;
    }
    if(!m_worldAudited) {
        m_worldSupported=auditWorld(pl); m_worldAudited=true;
    }
    if(!m_worldSupported) { m_status="Level has unsupported objects / triggers"; return false; }
    if(m_dx<=0 || !std::isfinite(m_dx)) { m_status="Warming up"; return false; }
    if(m_stepDt<=0 || m_inverted!=p->m_isUpsideDown || m_size!=p->m_vehicleSize ||
       m_speed!=p->m_playerSpeed || std::abs(pl->m_gameState.m_cameraAngle)>0.01f ||
       std::abs(pl->m_gameState.m_timeWarp-1.0f)>0.001f) {
        m_status="Physics epoch / camera / timewarp changed"; return false;
    }
    if(std::abs(p->getRotationX()-p->getRotationY())>0.01f) {
        m_status="Unsupported rotation"; return false;
    }
    return true;
}
State Assist::state(PlayerObject* p) {
    auto pos=p->getPosition();
    auto box=p->getObjectRect();
    // AABB is intentionally conservative; NOT GD's exact inner Wave hitbox.
    return {pos.x,pos.y,m_dx,m_dx*(p->m_vehicleSize<0.99f?2.0:1.0),
        std::max(0.5,static_cast<double>(box.size.width)*0.5),
        std::max(0.5,static_cast<double>(box.size.height)*0.5),
        p->buttonDown(PlayerButton::Jump),p->m_isUpsideDown};
}
std::optional<World> Assist::world(PlayLayer* pl,State const& s,int range) {
    World w;
    double span=s.dx*(range+2)+s.halfWidth+40;
    double left=s.x-span, right=s.x+span*2;
    // Conservative lower world bound. Other camera/ground configurations
    // are not reproduced by this model (documented alpha limitation).
    w.obstacles.push_back({left-100,-1e7,right+100,0});
    CCObject* raw=nullptr;
    CCARRAY_FOREACH(pl->m_objects,raw) {
        auto o=static_cast<GameObject*>(raw);
        if(o==pl->m_player1 || o==pl->m_player2 || o->m_isDisabled ||
           o->m_isDisabled2 || o->m_isGroupDisabled || o->m_isGroupDisabledTemp ||
           o->m_isNoTouch || o->m_objectType==GameObjectType::Decoration) continue;
        auto r=o->getObjectRect();
        if(r.getMaxX()<left || r.getMinX()>right) continue;
        if(w.obstacles.size()+w.barriers.size()>2048) return std::nullopt;
        Rect box{r.getMinX(),r.getMinY(),r.getMaxX(),r.getMaxY()};
        if(!std::isfinite(box.left)||!std::isfinite(box.bottom)||
           !std::isfinite(box.right)||!std::isfinite(box.top)||
           box.right<box.left||box.top<box.bottom) return std::nullopt;
        if(o->m_objectType==GameObjectType::Solid || o->m_objectType==GameObjectType::Hazard) {
            if(o->m_isPassable) w.barriers.push_back(box);
            else w.obstacles.push_back(box);
        } else {
            // Portals invalidate prediction across the full vertical corridor.
            w.barriers.push_back({box.left,-1e7,box.right,1e7});
        }
    }
    return w;
}
bool Assist::input(PlayLayer* pl,bool down) {
    bind(pl);
    auto p=pl->m_player1; if(!p) return false;
    auto cfg=Config::read();
    if(!compatible(pl,p,cfg) || (down?!cfg.presses:!cfg.releases)) {
        finishDeath(pl); flush(p); m_history.clear(); return false;
    }
    // Preserve physical edge order. Never leave a delayed release stuck.
    // An opposite edge arriving before a planned edge flushes both in order.
    if(m_pending) {
        flush(p); apply(p,down); m_history.clear();
        m_status="Rapid input: correction bypassed"; return true;
    }
    auto current=state(p);
    if(current.held==down) return false; // repeat / duplicate edge
    int n=boundedWindow(cfg.window), horizon=n+12;
    auto scene=world(pl,current,horizon+n);
    if(!scene) { m_history.clear(); return false; }
    auto forward=future(current,down,n,horizon,*scene);
    // Don't modify a safe immediate input. During collision grace, a positive
    // delay is not enough: only a valid historical replacement may rescue it.
    if(forward && *forward==0 && !m_grace.active) {
        m_history.clear(); m_status="Input unchanged"; return false;
    }
    if(cfg.rollback) {
        for(auto it=m_history.rbegin();it!=m_history.rend();++it) {
            int age=static_cast<int>(m_tick-it->tick);
            if(age<=0) continue;
            if(age>n) break;
            // A ghost state AFTER the first lethal contact cannot be used to
            // clear grace: it would merely bless a noclip passage.
            if(m_grace.active && it->tick>m_collisionAnchor) continue;
            if(!m_grace.active && forward && age>=*forward) break; // forward wins ties
            auto proposal=past(it->state,down,age,n,horizon,*scene);
            if(!proposal) continue;
            auto saved=it->checkpoint; auto present=proposal->present;
            // LOCAL PLAYER rewind: world clock/audio/triggers are NOT rewound.
            // Hence Practice-only + whole-level static audit, and alpha label.
            p->loadFromCheckpoint(saved.data());
            apply(p,down);
            p->setPosition({static_cast<float>(present.x),static_cast<float>(present.y)});
            auto previous=present;
            previous.x-=present.dx;
            previous.y-=(present.held!=present.inverted?1.0:-1.0)*present.rise;
            p->m_lastPosition={static_cast<float>(previous.x),static_cast<float>(previous.y)};
            p->resetStreak();
            m_grace.clear(); m_deathObject=nullptr; m_history.clear();
            m_status="Local rewind: -"+std::to_string(age)+" ticks (experimental)";
            return true;
        }
    }
    m_history.clear(); // never move a later edge across this physical edge
    if(m_grace.active) { finishDeath(pl); return false; }
    if(forward && *forward>0) {
        m_pending=Pending{m_tick+static_cast<uint64_t>(*forward),down,*forward};
        m_status="Scheduled: +"+std::to_string(*forward)+" ticks";
        return true;
    }
    m_status="No predicted safe offset";
    return false;
}
void Assist::before(PlayerObject* p,float dt) {
    auto pl=PlayLayer::get();
    if(!pl || pl->m_player1!=p) return;
    bind(pl); auto cfg=Config::read();
    if(m_haveConfig && (cfg.enabled!=m_last.enabled || cfg.rollback!=m_last.rollback ||
       cfg.window!=m_last.window || cfg.presses!=m_last.presses || cfg.releases!=m_last.releases)) {
        finishDeath(pl); flush(p); m_history.clear();
    }
    m_last=cfg; m_haveConfig=true;
    // History is invalid after a mode/size/gravity/speed/timestep change.
    bool changed=m_stepDt>0 && (std::abs(dt-m_stepDt)>1e-5 ||
        m_inverted!=p->m_isUpsideDown || m_size!=p->m_vehicleSize || m_speed!=p->m_playerSpeed);
    if(changed) { finishDeath(pl); flush(p); m_history.clear(); m_dx=0; }
    m_stepDt=dt; m_inverted=p->m_isUpsideDown;
    m_size=p->m_vehicleSize; m_speed=p->m_playerSpeed;
    if(!compatible(pl,p,cfg)) {
        finishDeath(pl); flush(p); m_history.clear();
    } else {
        if(m_pending && m_tick>=m_pending->due) {
            auto edge=*m_pending; m_pending.reset(); apply(p,edge.down);
            m_history.clear(); m_status="Applied: +"+std::to_string(edge.offset)+" ticks";
        }
        if(cfg.rollback) {
            auto cp=PlayerCheckpoint::create();
            p->saveToCheckpoint(cp);
            m_history.push_back({m_tick,state(p),cp});
            while(m_history.size()>11) m_history.pop_front();
        }
    }
    m_before=p->getPosition();
}
void Assist::after(PlayerObject* p,float) {
    auto pl=PlayLayer::get(); if(!pl || m_owner!=pl || pl->m_player1!=p) return;
    double dx=p->getPositionX()-m_before.x;
    // A teleport/forced displacement invalidates the linear predictor.
    if(dx>0 && dx<25 && std::isfinite(dx)) {
        if(m_dx>0 && std::abs(dx-m_dx)>0.02) m_history.clear();
        m_dx=dx;
    } else { m_dx=0; m_history.clear(); }
    ++m_tick;
    if(m_grace.active && m_grace.advance(Config::read().window)) finishDeath(pl);
}
bool Assist::interceptDeath(PlayLayer* pl,PlayerObject* p,GameObject* o) {
    if(m_bypassDeath || m_owner!=pl || p!=pl->m_player1) return false;
    auto cfg=Config::read();
    if(!cfg.rollback || !compatible(pl,p,cfg) || m_pending || m_history.empty() ||
       !o || (o->m_objectType!=GameObjectType::Solid && o->m_objectType!=GameObjectType::Hazard))
        return false;
    if(!m_grace.active) m_collisionAnchor=m_tick>0?m_tick-1:0;
    m_grace.begin(); // never reset on repeated collisions
    if(!m_deathObject) m_deathObject=o;
    m_status="Late-input grace (temporary collision suppression)";
    return true;
}
}
