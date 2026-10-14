#include "../../../src/status_region_cache.h"
#include <cassert>
#include <cstdio>
int main(){status_memory::Cache c;unsigned calls=0;bool writing=true;auto q=[&](uintptr_t a,status_memory::Region&r){calls++;uintptr_t start=a&~uintptr_t(4095);r={start,start+4096,true,writing};return true;};
 for(int i=0;i<200;i++)assert(c.allows((void*)(uintptr_t)(0x10020+i*8),16,false,q));assert(calls==1&&c.hits==199);
 assert(c.allows((void*)0x10ff0,32,false,q));assert(calls==2);assert(c.allows((void*)0x10020,8,true,q));assert(calls==2);
 c.reset();writing=false;assert(!c.allows((void*)0x10020,8,true,q));assert(calls==3);assert(c.allows((void*)0x10020,8,false,q));assert(calls==3);
 assert(!c.allows((void*)1,8,false,q));assert(!c.allows((void*)(UINTPTR_MAX-2),8,false,q));
 c.reset();writing=true;assert(c.allows((void*)0x10020,8,true,q));assert(calls==4); // no cross-tick stale read-only cache
 c.reset();for(unsigned i=0;i<100;i++)assert(c.allows((void*)(uintptr_t)(0x10000+i*4096),8,false,q));assert(c.count==64);
 auto bad=[](uintptr_t a,status_memory::Region&r){r={a,a,false,false};return true;};c.reset();assert(!c.allows((void*)0x10020,8,false,bad));
 std::printf("PASS 200 shared-region reads: 200 -> 1 queries; cross-region, writes, reset, overflow, capacity guards\n");}
