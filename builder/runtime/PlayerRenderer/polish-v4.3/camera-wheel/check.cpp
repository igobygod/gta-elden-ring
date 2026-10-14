#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "../../src/camera_overlay_policy.h"
#include "../../src/arsenal_wheel_policy.h"
int main(){
 camera_overlay::HeldView h;float m[16]={1,0,0,0,0,1,0,0,0,0,1,0,1,3,-2,1},p[3]={0,1,0},out[16]={},f=0;
 assert(!h.restore(1,p,out,f));h.save(1,m,p,14);assert(h.restore(1,p,out,f));assert(!memcmp(out,m,sizeof(m))&&f==14);
 for(int tick=0;tick<1200;tick++){assert(h.restore(1,p,out,f));assert(!memcmp(out,m,sizeof(m))&&f==14);}
 float moved[3]={.1f,1.2f,.3f};assert(h.restore(1,moved,out,f));assert(fabsf(out[12]-1.1f)<.0001f&&fabsf(out[13]-3.2f)<.0001f&&fabsf(out[14]+1.7f)<.0001f);
 assert(!h.restore(2,p,out,f));float bad[3]={NAN,1,0};assert(!h.restore(1,bad,out,f));bad[0]=5;assert(!h.restore(1,bad,out,f));h.reset();assert(!h.restore(1,p,out,f));
 for(unsigned sector=0;sector<8;sector++){float angle=sector*.7853981634f,x=0,y=0;unsigned want=arsenal_wheel::weapon_at_sector(sector);assert(arsenal_wheel::cursor_update(x,y,sinf(angle)*100,-cosf(angle)*100,4)==want);assert(arsenal_wheel::sector_of_weapon(want)==sector);}
 float x=0,y=0;assert(arsenal_wheel::cursor_update(x,y,0,0,4)==4);assert(arsenal_wheel::cursor_update(x,y,0,-100000,4)==1);assert(fabsf(y+100)<.001f);assert(arsenal_wheel::cursor_update(x,y,100,100,1)==4);assert(arsenal_wheel::cursor_update(x,y,NAN,0,4)==4);
 arsenal_wheel::LauncherScroll s;assert(s.update(60,true,6)==0);assert(s.update(60,true,6)==1);assert(s.update(-120,true,6)==0);assert(s.update(120,true,4)==0);
 puts("PASS camera modal hold: 1200 unchanged frames, scope FOV, movement, owner/teleport/NaN/reset gates; GTA 8-sector mapping, cursor clamp/reversal/deadzone and launcher scrolling");
}
