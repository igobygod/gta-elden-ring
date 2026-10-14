#pragma once
#include <cmath>
namespace lock_camera_framing {
// Keep ER's solved orientation, distance along the view direction and vertical
// composition. Translate only on its right axis to put the player's torso on
// the horizontal centre line. Collision may subsequently shorten pivot->wish;
// every point on that line retains the same horizontal framing.
inline bool centered_origin(const float*view,const float*pivot,float*wish){
 if(!view||!pivot||!wish)return false;
 for(unsigned i=0;i<16;i++)if(!std::isfinite(view[i]))return false;
 float square=0,lateral=0;
 for(unsigned i=0;i<3;i++){
  if(!std::isfinite(pivot[i]))return false;
  square+=view[i]*view[i];lateral+=(pivot[i]-view[12+i])*view[i];
 }
 if(!std::isfinite(square)||!std::isfinite(lateral)||square<.000001f)return false;
 lateral/=square;
 for(unsigned i=0;i<3;i++){wish[i]=view[12+i]+view[i]*lateral;if(!std::isfinite(wish[i]))return false;}
 return true;
}
}
