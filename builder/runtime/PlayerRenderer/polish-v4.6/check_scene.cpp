#include "../src/scene_integration.h"
#include <cassert>
#include <cstdio>
struct Vertex{float x=3,y=4,z=5,w=1,u=.2f,v=.3f,r=1,g=1,b=1,a=.8f,nu=0,nv=0,frameBlend=0,softness=0;};
struct Point{float x,y,z;};
int main(){
 float cam[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
 float normals[9]={0,0,3,4,0,0,0,0,0};
 for(int degree=0;degree<360;degree++){
  float a=degree*.01745329252f,f[]={std::sin(a),std::cos(a)};std::vector<Vertex> v(3);
  scene_integration::body_normals(v,normals,f,cam);
  for(auto p:v){assert(std::fabs(p.r*p.r+p.g*p.g+p.b*p.b-1)<.00001);assert(p.x==3&&p.y==4&&p.z==5&&p.u==.2f&&p.a==.8f);}
  assert(std::fabs(v[0].g-1)<1e-5&&v[0].nu==1);assert(std::fabs(v[1].r+f[1])<1e-5&&std::fabs(v[1].b-f[0])<1e-5);
 }
 scene_integration::FootContacts feet;
 std::vector<Point> rest={{-.15f,0,-.92871094f},{.15f,0,-.92871094f},{0,0,1}};feet.prepare(rest);
 float pose[]={-.15f,0,-.92871094f,.15f,0,-.52871094f,0,0,1};
 auto left=feet.sample(0,pose,3,0,0),right=feet.sample(1,pose,3,0,0);
 assert(left.valid&&left.opacity>.47f&&!right.valid&&right.opacity==0);
 assert(!feet.sample(0,pose,3,.6f,0).valid);assert(!feet.sample(0,pose,3,-.2f,0).valid);
 assert(!feet.sample(0,pose,2,0,0).valid);
 puts("PASS scene normals: 360 turns preserve geometry/UV/alpha; sole contact follows planted foot and rejects raised, airborne, below-floor and stale geometry.");
}
