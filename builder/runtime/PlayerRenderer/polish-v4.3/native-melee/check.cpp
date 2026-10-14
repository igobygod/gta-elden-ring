#include <cassert>
#include <cstdio>
#include "../../src/native_melee_policy.h"
#include "../../src/arsenal_wheel_policy.h"
using namespace native_melee_policy;
int main(){
 EquipmentLease l;uint32_t target=0;l.owner_changed(100);assert(l.owner==100&&!l.pending());
 // Genuine native fist/sword IDs, prior shield + crossbow restored independently.
 l.applied(0,30000000,fists);l.applied(1,43000000,sword);assert(l.pending());
 assert(l.restore_target(0,fists,target)&&target==30000000);
 assert(l.restore_target(1,sword,target)&&target==43000000);
 // Sword <-> fists retains the original right-hand weapon, not the intermediate sword.
 l.applied(1,sword,fists);assert(l.restore_target(1,fists,target)&&target==43000000);
 l.applied(1,fists,sword);assert(l.restore_target(1,sword,target)&&target==43000000);
 // Manual equip changes belong to the player; restore must not overwrite them.
 assert(!l.restore_target(0,30100000,target));assert(!l.hands[0].owned);
 l.applied(0,30100000,fists);assert(l.restore_target(0,fists,target)&&target==30100000);
 // A changed selected slot preserves all earlier leased slots for later restoration.
 l.applied(3,44000000,sword);assert(l.restore_target(1,sword,target)&&target==43000000);
 assert(l.restore_target(3,sword,target)&&target==44000000);
 l.restored(0);l.restored(1);l.restored(3);assert(!l.pending());
 l.applied(1,sword,sword);assert(!l.pending());
 l.applied(1,0xFFFFFFFFu,fists);assert(l.restore_target(1,fists,target)&&target==fists);
 assert(restorable_item(0)==fists);assert(restorable_item(2140000)==sword);
 l.grant_attempts=3;l.owner_changed(100);assert(l.pending()&&l.grant_attempts==3);
 l.owner_changed(0);assert(l.owner==100&&l.pending()&&l.grant_attempts==3);
 l.owner_changed(200);assert(l.owner==200&&!l.pending()&&!l.grant_attempts);
 l.applied(6,1,sword);assert(!l.pending()&&!l.restore_target(6,sword,target));
 unsigned left=0,right=0;for(unsigned style=0;style<4;style++)for(unsigned ls=0;ls<3;ls++)for(unsigned rs=0;rs<3;rs++){assert(slots(style,ls,rs,left,right));assert(left==ls*2&&right==rs*2+1);}
 assert(!slots(4,0,0,left,right)&&!slots(1,3,0,left,right)&&!slots(1,0,3,left,right));
 arsenal_wheel::MeleeScroll m;arsenal_wheel::LauncherScroll r;
 assert(m.update(60,true,0)==0);assert(m.update(60,true,0)==1);assert(m.update(-120,true,0)==0);
 assert(m.update(120,true,6)==0);assert(r.update(120,true,0)==0);
 m.update(60,true,0);m.update(0,true,4);assert(m.update(60,true,0)==0);
 m.reset();assert(m.update(120,true,0)==1);assert(m.update(120,false,0)==1);
 assert(r.update(120,true,6)==1&&m.variant==1);
 m.reset();assert(m.update(240,true,0)==1);assert(m.update(-120,true,0)==0);
 printf("PASS native melee: both-hand restoration, melee variant changes, manual equipment ownership, selected-slot changes, native empty-hand mapping, owner changes, 36 valid/3 invalid slot layouts, independent melee/launcher wheel scrolling\n");
}
