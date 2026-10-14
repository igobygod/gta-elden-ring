#include <initializer_list>
#include "../../src/scope_reticle.h"
#include <cassert>
#include <cstdio>
#include <limits>
int main(){unsigned cases=0;
 for(float radius:{224.f,299.f,448.f,600.f})for(int ix=-6;ix<=6;ix++)for(int iy=-6;iy<=6;iy++){
  float x=radius*ix/10,y=radius*iy/10;if(x*x+y*y>radius*radius*.64f)continue;
  unsigned n=0,centre=0;scope_reticle::draw(radius,x,y,[&](float l,float t,float r,float b,float c){
   assert(l<=r&&t<=b);for(float xx:{l,r})for(float yy:{t,b})assert(xx*xx+yy*yy<=radius*radius+.01f);
   if(c==1){assert(std::fabs((l+r)/2-x)<.001f&&std::fabs((t+b)/2-y)<.001f);centre++;}n++;
  });assert(n>=3&&centre==1);cases++;
 }
 bool bad=false;scope_reticle::draw(300.f,std::numeric_limits<float>::quiet_NaN(),0.f,[&](float,float,float,float,float){bad=true;});assert(!bad);
 printf("PASS scope crosshair remains aligned and within lens: %u positions\n",cases);
}
