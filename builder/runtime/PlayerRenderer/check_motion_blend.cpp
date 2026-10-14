#include <initializer_list>

#include "src/motion_blend.h"
#include "src/native_animation_policy.h"
#include "src/directional_motion.h"
#include <cassert>
int main(){
 assert(!native_animation_policy::control_action(27110));
 assert(native_animation_policy::camera_action(27110));
 assert(native_animation_policy::camera_action(202000));
 assert(!native_animation_policy::camera_action(30000));
 MouseAimOffset aim;aim.add(100,-50);assert(aim.yaw>0&&aim.pitch>0);
 aim.add(100000,-100000);assert(aim.yaw==MouseAimOffset::maxYaw&&aim.pitch==MouseAimOffset::maxPitch);
 aim.release(1.f/60);assert(aim.yaw<MouseAimOffset::maxYaw&&aim.yaw>0);
 aim.release(.3f);assert(aim.yaw==0&&aim.pitch==0);
 constexpr float pi=3.14159265358979323846f;
 assert(std::fabs(donor_motion::torsoAimYaw(0,pi))<1.23f);
 assert(std::fabs(std::fabs(donor_motion::torsoAimYaw(0,pi))-1.22f)<.001f);
 assert(std::fabs(donor_motion::torsoAimYaw(pi-.1f,-pi+.1f)+.2f)<.001f);
 // Positive mouse X must project camera-forward toward camera-right.
 MouseAimOffset cameraTurn;cameraTurn.add(100,0);float cameraRight[3]={1,0,0},cameraForward[3]={0,0,1};cameraTurn.rotateYaw(cameraRight,cameraForward);
 assert(cameraForward[0]>0&&cameraForward[0]*1.f+cameraForward[2]*0.f>0);
 // Renderer worldPoint maps donor XY to ER (-X,-Z), then rotates by bodyYaw.
 // A donor +Y forward at body yaw 0 needs +Z donor yaw to aim toward ER +X.
 auto projectedForward=[](float bodyYaw,float donorYaw,float out[2]){
  float donorX=-std::sin(donorYaw),donorY=std::cos(donorYaw);
  float localX=-donorX,localZ=-donorY,c=std::cos(bodyYaw),s=std::sin(bodyYaw);
  out[0]=localX*c+localZ*s;out[1]=-localX*s+localZ*c;
 };
 const float bodyAngles[2]={0.f,.3f},aimAngles[2]={-pi*.5f,-.1f};
 for(int i=0;i<2;i++){
  float yaw=donor_motion::torsoAimYaw(bodyAngles[i],aimAngles[i]),projected[2];projectedForward(bodyAngles[i],yaw,projected);
  float target[2]={-std::sin(aimAngles[i]),-std::cos(aimAngles[i])};
  assert(projected[0]*target[0]+projected[1]*target[1]>.9f);
 }
 for(float dt:{1.f/30,1.f/60,1.f/120}){PoseTransition b;assert(b.update(1,0,0,dt,false).reset);auto jump=b.update(1,202000,0,dt,false);assert(jump.capture&&jump.alpha==0);float last=0;for(int i=1;i<30;i++){auto r=b.update(1,202000,i*dt,dt,false);assert(!r.capture&&r.alpha>=last);last=r.alpha;}assert(last==1);auto land=b.update(1,202100,0,dt,false);assert(land.capture&&land.alpha==0);auto run=b.update(1,0,0,dt,false);assert(run.capture&&run.alpha==0);auto roll=b.update(1,27110,0,dt,false);assert(roll.capture);b.update(1,27110,.5f,dt,false);assert(b.update(1,27110,0,dt,false).capture);assert(b.update(2,0,0,dt,false).reset);assert(b.update(2,0,0,dt,true).reset);}
}
