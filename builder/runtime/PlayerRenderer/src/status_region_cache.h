#pragma once
#include <stdint.h>
#include <stddef.h>
namespace status_memory {
struct Region {uintptr_t first,end;bool readable,writable;};
struct Cache {
 Region entries[64];unsigned count=0,queries=0,hits=0;
 void reset(){count=queries=hits=0;}
 template<class Query>bool allows(const void*p,size_t size,bool writing,Query query){
  uintptr_t a=(uintptr_t)p,end=a+size;if(a<0x10000||end<a)return false;
  while(a<end){Region r={};bool found=false;for(unsigned i=0;i<count;i++)if(a>=entries[i].first&&a<entries[i].end){r=entries[i];found=true;hits++;break;}
   if(!found){queries++;if(!query(a,r)||r.first>a||r.end<=a)return false;if(count<64)entries[count++]=r;}
   if(writing?!r.writable:!r.readable)return false;a=r.end;
  }return true;
 }
};
}
