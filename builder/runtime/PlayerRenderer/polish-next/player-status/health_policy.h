#pragma once
#include <cmath>
#include <cstdint>
namespace status_health {
inline bool valid(int hp,int maximum){return maximum>0&&maximum<=1000000&&hp>=0&&hp<=maximum;}
inline int scaled(int hp,int maximum,int target){
 if(!valid(hp,maximum)||target<10||target>1000000)return -1;
 if(hp==0)return 0;
 int value=int(std::llround(double(hp)*target/maximum));return value<1?1:value>target?target:value;
}
struct Edge {bool down=false;bool take(bool current,bool allowed){bool edge=current&&!down;down=current;return edge&&allowed;}};
inline bool hostile(unsigned team,int hp,float squaredDistance){return (team==6||team==7)&&hp>0&&std::isfinite(squaredDistance)&&squaredDistance<=80.f*80.f&&squaredDistance>=0;}
}
