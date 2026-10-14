#include "../../src/lock_marker_policy.h"
#include "../../src/lock_camera_framing.h"
#include <cassert>
#include <initializer_list>
#include <cstdlib>
#include <cstdio>
#include <limits>
#include <cstring>
using lock_marker::Data;
void basis(float yaw,float pitch,float*view){
 memset(view,0,64);view[0]=cosf(yaw);view[2]=-sinf(yaw);view[4]=sinf(yaw)*sinf(pitch);view[5]=cosf(pitch);view[6]=cosf(yaw)*sinf(pitch);view[8]=sinf(yaw)*cosf(pitch);view[9]=-sinf(pitch);view[10]=cosf(yaw)*cosf(pitch);view[12]=14;view[13]=9;view[14]=-8;view[15]=1;
}
int main(){
 unsigned cases=0;Data src{1,321,654},out{};
 for(float fov:{6.f,14.f,42.f,55.f})for(float aspect:{1.f,4.f/3,16.f/9,2.4f})for(float yaw:{-2.f,0.f,1.1f})for(float pitch:{-.5f,0.f,.4f})for(bool center:{false,true}){
  float view[16];basis(yaw,pitch,view);if(center){float pivot[3]={15,7,-4},origin[3];assert(lock_camera_framing::centered_origin(view,pivot,origin));memcpy(view+12,origin,12);}
  float rad=fov*.01745329251994f,t=tanf(rad*.5f);
  for(float nx:{-.6f,0.f,.7f})for(float ny:{-.6f,0.f,.7f}){
   float target[3];for(int i=0;i<3;i++)target[i]=view[12+i]+view[8+i]*20+view[i]*nx*20*t*aspect+view[4+i]*ny*20*t;
   for(unsigned width:{800u,1920u,3840u}){unsigned height=unsigned(width/aspect);
    assert(lock_marker::project(src,target,view,rad,aspect,width,height,out));
    assert(out.state==src.state&&src.x==321&&src.y==654);
    assert(abs(out.x-int((.5f+.5f*nx)*width))<=1&&abs(out.y-int((.5f-.5f*ny)*height))<=1);
    cases++;
   }
  }
 }
 float view[16];basis(0,0,view);float target[3]={14,9,12};
 auto reject=[&](const float*point,const float*matrix,float fov,float aspect,unsigned w,unsigned h){Data saved{17,18,19};assert(!lock_marker::project(src,point,matrix,fov,aspect,w,h,saved));assert(saved.state==17&&saved.x==18&&saved.y==19);};
 float nan=std::numeric_limits<float>::quiet_NaN(),bad[16];memcpy(bad,view,64);bad[4]=nan;
 reject(target,bad,.7f,1.7f,1920,1080);reject(target,view,nan,1.7f,1920,1080);reject(target,view,.7f,nan,1920,1080);
 reject(target,view,.7f,1.7f,0,1080);reject(target,view,.7f,1.7f,1920,20000);
 float behind[3]={14,9,-12},offscreen[3]={1000,9,12},invalid[3]={nan,9,12};
 reject(behind,view,.7f,1.7f,1920,1080);reject(offscreen,view,.7f,1.7f,1920,1080);reject(invalid,view,.7f,1.7f,1920,1080);
 ShoulderAimSnapshot snapshot{};shoulder_aim::Offset offset;offset.add(120,-90,1.7f,false);
 assert(shoulder_aim::make(snapshot,123,1000,4,view,.7f,1.7f,offset,true,false));
 lock_marker::Gate gate;gate.normal=true;gate.loading=gate.fade=gate.modal=gate.dead=false;gate.native_locked=true;gate.weapon=4;gate.owner=123;gate.now=1001;
 assert(lock_marker::eligible(src,gate,snapshot,view,.7f,1.7f));
 for(unsigned field=0;field<10;field++){auto g=gate;switch(field){case 0:g.normal=false;break;case 1:g.loading=true;break;case 2:g.fade=true;break;case 3:g.modal=true;break;case 4:g.dead=true;break;case 5:g.native_locked=false;break;case 6:g.weapon=0;break;case 7:g.owner=456;break;case 8:g.now=1101;break;case 9:g.now=999;break;}assert(!lock_marker::eligible(src,g,snapshot,view,.7f,1.7f));}
 auto unset=snapshot;unset.ready=0;assert(!lock_marker::eligible(src,gate,unset,view,.7f,1.7f));
 unset=snapshot;unset.locked=0;assert(!lock_marker::eligible(src,gate,unset,view,.7f,1.7f));
 assert(!lock_marker::eligible(src,gate,snapshot,view,.8f,1.7f));assert(!lock_marker::eligible(src,gate,snapshot,view,.7f,1.8f));
 memcpy(bad,view,64);bad[12]+=.1f;assert(!lock_marker::eligible(src,gate,snapshot,bad,.7f,1.7f));
 assert(!lock_marker::eligible(Data{0,0,0},gate,snapshot,view,.7f,1.7f));assert(!lock_marker::eligible(Data{2,0,0},gate,snapshot,view,.7f,1.7f));
 // Independent mouse ray offset must not displace the target-owned white dot.
 assert(snapshot.ndc[0]!=0);assert(lock_marker::project(src,target,view,.7f,1.7f,1920,1080,out));assert(out.x==960&&out.y==540);
 // Regression for the live1280x720 viewport: physical dimensions must not
 // shrink a movie-space marker a second time. Actual native writer constants
 // are separately extracted/checked from the installed executable.
 uint32_t stageW=0,stageH=0;
 assert(lock_marker::stage_extent(1920.f,1080.f,stageW,stageH));
 assert(stageW==1920&&stageH==1080);
 for(float fbWidth:{800.f,1280.f,1920.f,2560.f,3840.f}){
  float fbHeight=fbWidth*9.f/16.f;
  // Mirrors native7780F6..77811E: ratio cancellation is intentional.
  float nativeWidth=(1920.f/fbWidth)*fbWidth;
  float nativeHeight=(1080.f/fbHeight)*fbHeight;
  assert(lock_marker::project(src,target,view,.7f,1.7f,stageW,stageH,out));
  assert(abs(float(out.x)-nativeWidth*.5f)<.01f&&abs(float(out.y)-nativeHeight*.5f)<.01f);
  float screenX=out.x*fbWidth/1920.f,screenY=out.y*fbHeight/1080.f;
  assert(abs(screenX-fbWidth*.5f)<.01f&&abs(screenY-fbHeight*.5f)<.01f);
 }
 Data broken{};assert(lock_marker::project(src,target,view,.7f,1.7f,1280,720,broken));
 assert(broken.x*1280.f/1920.f!=640.f); // V4.7's observed double scaling.
 assert(broken.y*720.f/1080.f!=360.f);
 assert(!lock_marker::stage_extent(nan,1080,stageW,stageH));
 assert(!lock_marker::stage_extent(1920,.5f,stageW,stageH));
 assert(!lock_marker::stage_extent(1920.5f,1080,stageW,stageH));
 printf("PASS %u final-camera/FOV/aspect/canvas target projections; lifecycle and malformed-input guards\n",cases);
}
