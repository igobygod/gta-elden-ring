#pragma once
#include <cmath>
// The scope lens stays centred on the locked camera. Its aiming cross moves
// independently, with every line clipped inside the existing circular lens.
namespace scope_reticle {
template<class Draw> void draw(float radius,float x,float y,Draw rect){
 if(!std::isfinite(radius)||!std::isfinite(x)||!std::isfinite(y)||radius<10)return;
 const float r=radius-2.f;
 if(x*x+y*y>(r-6)*(r-6))return;
 float horizontal=std::sqrt(std::fmax(0.f,r*r-(std::fabs(y)+.6f)*(std::fabs(y)+.6f)));
 float vertical=std::sqrt(std::fmax(0.f,r*r-(std::fabs(x)+.6f)*(std::fabs(x)+.6f)));
 rect(-horizontal,y-.6f,horizontal,y+.6f,0.f);
 rect(x-.6f,-vertical,x+.6f,vertical,0.f);
 auto inside=[&](float l,float t,float rr,float b){float xx=std::fmax(std::fabs(l),std::fabs(rr)),yy=std::fmax(std::fabs(t),std::fabs(b));return xx*xx+yy*yy<=r*r;};
 for(int n=-3;n<=3;n++)if(n){float d=n*radius/5;
  if(inside(x+d-1,y-5,x+d+1,y+5))rect(x+d-1,y-5,x+d+1,y+5,0.f);
  if(inside(x-5,y+d-1,x+5,y+d+1))rect(x-5,y+d-1,x+5,y+d+1,0.f);
 }
 rect(x-1,y-1,x+1,y+1,1.f);
}
}
