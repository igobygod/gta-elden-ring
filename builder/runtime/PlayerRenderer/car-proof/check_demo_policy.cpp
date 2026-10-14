#include "car_demo_policy.h"
#include <cassert>
#include <cstdio>
int main(){
    using namespace car_demo_policy;
    float heights[5]={2,2.01f,2.03f,1.98f,2},high=0,spread=0;
    assert(ground_height(heights,high,spread)&&high==2.03f&&spread<.12f);
    heights[4]=2.5f;assert(ground_height(heights,high,spread));
    heights[4]=3.f;assert(!ground_height(heights,high,spread));
    heights[4]=NAN;assert(!ground_height(heights,high,spread));
    float a[3]={0,0,0},b[3]={2,0,0};
    assert(lifecycle(1,1,true,false,5,5,a,b));
    assert(!lifecycle(1,2,true,false,5,5,a,b));
    assert(!lifecycle(1,1,true,true,5,5,a,b));
    assert(!lifecycle(1,1,true,false,5,6,a,b));
    b[0]=8.01f;assert(!lifecycle(1,1,true,false,5,5,a,b));
    assert(fresh(1100,1000)&&!fresh(1300,1000)&&!fresh(999,1000)&&!fresh(1000,0));
    assert(world_ready(true,true,false,false));
    assert(!world_ready(true,true,true,false)&&!world_ready(true,true,false,true));
    assert(!world_ready(false,true,false,false)&&!world_ready(true,false,false,false));
    float forward[3]={0,0,1},origin[3]={10,2,20},matrix[16],front[3];
    transform(matrix,forward,origin);point(matrix,0,0,-2,front);
    assert(front[0]==10&&front[1]==2&&front[2]==22);
    float determinant=matrix[0]*(matrix[5]*matrix[10]-matrix[9]*matrix[6])-matrix[4]*(matrix[1]*matrix[10]-matrix[9]*matrix[2])+matrix[8]*(matrix[1]*matrix[6]-matrix[5]*matrix[2]);
    assert(std::fabs(determinant-1)<.0001f);
    std::puts("PASS parked car policy: flat/modest-slope footprint, lifecycle/warp/death/map, stale snapshots, loading/fade, proper -Z forward rotation");
}
