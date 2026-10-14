#include "lifecycle_policy.h"
#include <cassert>
#include <cstdio>
int main(){using namespace car_lifecycle;using vehicle::V;V shift;
 assert(elapsed(1000,1001)==0);assert(elapsed(2000,1000)==1000);
 assert(motion({0,.6f,0},{0,.6f,0},true,shift)==Motion::correction);
 assert(motion({24,.4f,-32},{24,.3f,-32},true,shift)==Motion::rebase&&shift.x==24&&shift.z==-32&&shift.y==0);
 assert(motion({8,0,0},{0,0,0},true,shift)==Motion::park);
 assert(motion({0,.05f,0},{},true,shift)==Motion::stable);
 assert(motion({19,0,0},{19,0,0},true,shift)==Motion::park);
 assert(motion({NAN,0,0},{},true,shift)==Motion::park);
 puts("PASS lifecycle: timer underflow, native contact correction, corroborated origin shift, real warp parks, invalid position");
}
