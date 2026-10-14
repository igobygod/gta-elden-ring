#include <cassert>
#include <cstdio>
#include "../../src/ground_contact_policy.h"
int main(){
 ground_contact::Cache cache;float point[3]={1,2,3};unsigned samples=0;
 // 60 FPS melee rendering republishes a real contact between bounded probes.
 for(uint64_t now=1000;now<2600;now+=16){
  if(cache.due(123,now)){cache.store(123,now,true,point,.2f);samples++;}
  assert(cache.valid(123,now));assert(cache.point[1]==2&&cache.height==.2f);
 }
 assert(samples==20); // 100 frames, one ray every 80 ms, not every frame.
 assert(!cache.valid(124,2600));assert(cache.due(124,2600));
 cache.store(124,2600,false,point,0);assert(!cache.valid(124,2600));
 assert(!cache.due(124,2679));assert(cache.due(124,2680));
 assert(cache.due(124,2500)&&!cache.valid(124,2500));
 cache.store(124,2680,true,point,0);assert(cache.valid(124,2879));assert(!cache.valid(124,2880));
 assert(cache.due(124,2681,true));cache.reset();assert(cache.due(124,2681)&&!cache.valid(124,2681));
 point[1]=NAN;cache.store(124,2681,true,point,0);assert(!cache.valid(124,2681));
 point[1]=2;cache.store(124,2681,true,point,INFINITY);assert(!cache.valid(124,2681));
 puts("PASS melee ground contact: cached publication across100 frames/20 probes; 80ms bound; owner, clock, miss, expiry, reset, malformed guards");
}
