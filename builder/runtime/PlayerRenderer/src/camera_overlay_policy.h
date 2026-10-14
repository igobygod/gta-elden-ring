#pragma once
#include <cstdint>
#include <cmath>
#include <cstring>
namespace camera_overlay {
// Modal text input owns input, never the camera. Keep a validated solved view
// across the console's active and key-drain phases; lifecycle gates own reset.
struct HeldView {
 uintptr_t owner=0;float matrix[16]={},position[3]={},fov=0;
 void reset(){owner=0;}
 void save(uintptr_t who,const float*view,const float*player,float angle){owner=who;std::memcpy(matrix,view,sizeof(matrix));std::memcpy(position,player,sizeof(position));fov=angle;}
 bool restore(uintptr_t who,const float*player,float*view,float&angle)const {
  if(!who||who!=owner)return false;float d=0;
  for(int i=0;i<3;i++){if(!std::isfinite(player[i]))return false;float v=player[i]-position[i];d+=v*v;}
  if(d>9.f)return false; // Teleport/load is a new view, not a held gameplay frame.
  std::memcpy(view,matrix,sizeof(matrix));for(int i=0;i<3;i++)view[12+i]+=player[i]-position[i];angle=fov;return true;
 }
};
}
