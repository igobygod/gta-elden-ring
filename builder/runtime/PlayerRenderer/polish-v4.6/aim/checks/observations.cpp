#include "../../../src/native_shot_observation.h"
#include <cassert>
#include <limits>
#include <cstdio>
#include <initializer_list>
using namespace native_shot_observation;
int main(){
 Ledger ledger;Request a;a.owner=123;a.bullet=98000000;a.begin=1000;a.end=1002;a.sequence=1;a.source=2;a.count=1;a.directions[0][2]=1;ledger.add(a);
 auto m=ledger.match(123,98000000,1020,.02f);assert(m.candidates==1&&m.request&&m.request->sequence==1);
 float aligned[3]={0,0,2000},redirected[3]={1000,0,1732.0508f};assert(nearest_angle(*m.request,aligned)<.001f);assert(std::abs(nearest_angle(*m.request,redirected)-30)<.001f);
 assert(!ledger.match(124,98000000,1020,.02f).request);assert(!ledger.match(123,98000200,1020,.02f).request);
 assert(!ledger.match(123,98000000,999,.02f).request);assert(!ledger.match(123,98000000,2000,.02f).request);
 assert(!ledger.match(123,98000000,1020,-1).request);assert(!ledger.match(123,98000000,1020,std::numeric_limits<float>::quiet_NaN()).request);
 auto b=a;b.begin=1030;b.end=1031;b.sequence=2;ledger.add(b);m=ledger.match(123,98000000,1040,.02f);assert(m.candidates==2&&!m.request);
 // A shotgun group contains eight deliberate spread rays. The observed ray
 // is compared to the bundle only after unique temporal association.
 Request shotgun=a;shotgun.bullet=98000400;shotgun.count=2;shotgun.directions[1][0]=.5f;shotgun.directions[1][2]=.8660254f;ledger.add(shotgun);
 m=ledger.match(123,98000400,1020,.02f);assert(m.request&&nearest_angle(*m.request,redirected)<.03f);
 // First-frame zero/tiny/overflow velocity remains ineligible, so the scanner
 // must not consume this bullet's one observation before initialized motion.
 float pending[3]={0,0,0};assert(observed_speed(pending)==0);pending[2]=.5f;assert(observed_speed(pending)==0);pending[2]=2000;assert(observed_speed(pending)==2000);pending[2]=std::numeric_limits<float>::max();assert(observed_speed(pending)==0);
 float invalid[3]={0,0,0};assert(nearest_angle(*m.request,invalid)==-1);invalid[0]=std::numeric_limits<float>::infinity();assert(nearest_angle(*m.request,invalid)==-1);
 // Fixed16slot capacity, no allocations and no unbounded request retention.
 for(unsigned i=0;i<32;i++){a.begin=2000+i*100;a.end=a.begin+1;a.sequence=10+i;ledger.add(a);}assert(!ledger.match(123,98000000,1020,.02f).request);
 for(int id:{98000000,98000200,98000400,98000500,98000600})assert(firearm(id));assert(!firearm(98000300)&&!firearm(98000700)&&!firearm(98000100));
 puts("PASS: native-shot evidence association, ambiguous burst rejection, measured 30-degree deviation detection, shotgun bundle, finite guards and bounded ledger");
}
