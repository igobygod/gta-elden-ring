#include "../src/ui_visibility_policy.h"
#include "../src/scene_lifecycle.h"
#include <cassert>
#include <cstdio>
int main(){
 SceneLifecycle life;
 assert(!life.update(1000,1,true,false,false,false,false));
 assert(life.update(1200,1,true,false,false,false,false));
 for(bool capture:{false,true}){
  assert(!ui_visibility::hide_world(3,false,true,capture));
  assert(!ui_visibility::hide_world(1,true,true,capture));
  assert(life.update(1300,1,true,false,false,ui_visibility::hide_world(1,true,true,capture),false));
  assert(ui_visibility::hide_world(-1,false,false,capture));
  assert(ui_visibility::hide_world(2,true,false,capture));
 }
 assert(!life.update(1400,1,true,true,false,false,false));
 puts("PASS reward visibility across capture loss; real menus/loading stay hidden");
}
