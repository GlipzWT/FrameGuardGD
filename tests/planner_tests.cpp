#include <frameguard/Planner.hpp>
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace frameguard;
int main() {
    int cases=0;
    auto check=[&](bool v) { ++cases; if (!v) { std::cerr << "Failed check " << cases << "\n"; std::exit(1); } };
    check(boundedWindow(-1)==0); check(boundedWindow(11)==10);
    check(boundedWindow(5)==5);
    check(intersects(0,0,10,0,{4,-1,5,1}));
    check(!intersects(0,2,10,2,{4,-1,5,1}));
    check(intersects(0,0,0,0,{-1,-1,1,1}));
    check(!intersects(0,0,0,0,{2,2,3,3}));
    check(intersects(0,0,10,10,{4,4,4,4}));
    check(intersects(10,10,0,0,{4,4,5,5}));
    check(intersects(0,0,10,0,{10,0,11,1}));
    State s{0,0,1,1,0,0,false,false}; World empty;
    check(step(s).y==-1);
    s.held=true; check(step(s).y==1);
    s.inverted=true; check(step(s).y==-1);
    s.held=false; check(step(s).y==1);
    s.inverted=false;
    check(future(s,true,5,10,empty)==0);
    check(future(s,true,0,10,empty)==0);
    // Press now hits upper wall at x=2; delaying for two ticks is safe.
    World upper{{{1.9,0.5,2.1,10}}, {}};
    auto d=future(s,true,5,10,upper);
    check(d && *d==1);
    check(!future(s,true,0,10,upper));
    check(!future(s,true,10,15,World{{{-1,-100,100,100}},{}}));
    // Release correction is symmetric.
    s.held=true;
    World lower{{{1.9,-10,2.1,-0.5}}, {}};
    check(future(s,false,5,10,lower)==1);
    // Unsupported regions are never considered safe.
    check(!future(s,false,5,10,World{{},{{-1,-100,100,100}}}));
    // Exact age bound for backward proposals; invalid ages cannot rewind.
    s.held=false;
    check(past(s,true,3,5,10,empty)->present.y==3);
    check(!past(s,true,0,5,10,empty));
    check(!past(s,true,6,5,10,empty));
    check(!past(s,true,1,0,10,empty));
    check(past(s,true,10,10,10,empty)->present.x==10);
    check(!past(s,true,11,10,10,empty));
    // Input three ticks too late: an earlier press avoids the lower wall.
    World late{{{1.9,-20,2.1,-0.5}}, {}};
    check(past(s,true,3,5,10,late).has_value());
    check(!simulate(s,true,3,10,late));
    // Swept collision and hitbox expansion, including between endpoints.
    State fast{0,0,10,0.01,0,0,true,false};
    check(!simulate(fast,true,0,1,World{{{4,-1,5,1}},{}}));
    State wide{0,0,1,1,2,2,true,false};
    check(!simulate(wide,true,0,1,World{{{2.5,0,3,1}},{}}));
    State bad=s; bad.dx=std::numeric_limits<double>::quiet_NaN();
    check(!future(bad,true,10,15,empty));
    bad=s; bad.dx=-1; check(!future(bad,true,10,15,empty));
    bad=s; bad.rise=0; check(!future(bad,true,10,15,empty));
    Grace g; g.begin(); check(!g.advance(3));
    g.begin(); check(g.elapsed==1); // repeated collisions do not reset grace
    check(!g.advance(3)); check(g.advance(3));
    g.clear(); check(!g.active && g.elapsed==0);
    // Every accepted forward proposal respects the selected window.
    for (int n=0;n<=10;++n) {
        auto r=future(s,true,n,16,upper);
        check(!r || (*r>=0 && *r<=n));
    }
    // Deterministic randomized oracle consistency: every reported proposal
    // is safe IN THIS MODEL, bounded, and has no nearer safe forward offset.
    unsigned seed=0x5eed1234;
    auto rand01=[&]() { seed=seed*1664525u+1013904223u; return (seed>>8)/16777216.0; };
    for(int run=0;run<2000;++run) {
        State a{0,0,0.2+rand01()*2.8,0.2+rand01()*2.8,0.1,0.1,
                rand01()>0.5,rand01()>0.5};
        World w;
        for(int j=0;j<3;++j) {
            double x=1+rand01()*30, y=-15+rand01()*30;
            w.obstacles.push_back({x,y,x+0.1+rand01()*3,y+0.1+rand01()*3});
        }
        int n=static_cast<int>(rand01()*11); bool desired=!a.held;
        auto result=future(a,desired,n,24,w);
        if(result) {
            check(*result>=0 && *result<=n);
            check(simulate(a,desired,*result,24,w).has_value());
            for(int d0=0;d0<*result;++d0) check(!simulate(a,desired,d0,24,w));
        } else {
            for(int d0=0;d0<=n;++d0) check(!simulate(a,desired,d0,24,w));
        }
    }
    std::cout << cases << " planner checks passed (including 2000 randomized scenes)\n";
}
