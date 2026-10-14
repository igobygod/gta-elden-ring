#pragma once
#include <stdint.h>
namespace native_locomotion_policy {
// These are return addresses of four verified calls to ChrIns::IsLockedOn.
// Camera, target selection, network serialization and TAE action consumers
// deliberately do not appear here. See polish-v4.8/movement/README.md.
inline int site(uintptr_t return_rva) {
 switch(return_rva) {
  case 0x3D9763:return 0; // PadManipulator target-facing direction
  case 0x3DA089:return 1; // PadManipulator target-relative movement angle
  case 0x3DBED0:return 2; // PadManipulator input/facing-vector update
  case 0x41F6F1:return 3; // Behavior graph IsLockon variable only
  default:return -1;
 }
}
inline bool locomotion(int32_t raw) {
 if(raw<0)return false;
 int32_t id=raw%1000000;
 // Idle plus native gait/start/stop/turn families. Never admit native attacks,
 // hurt/death, backsteps, rolls, jumps or special-action IDs.
 return id==0||(id>=20000&&id<23000);
}
inline bool fresh(uint64_t now,uint64_t then) {return then&&now>=then&&now-then<=100;}
struct State {
 uintptr_t owner=0,expected=0;
 uint64_t now=0,weapon_time=0,player_time=0;
 unsigned weapon=0;
 int32_t raw=-1;
 bool installed=false,alive=false,normal=false,modal=false,active=false,locked=false;
};
inline bool eligible(const State&s) {
 return s.installed&&s.owner&&s.owner==s.expected&&s.alive&&s.normal&&!s.modal&&
  s.active&&s.locked&&s.weapon>=1&&s.weapon<=7&&locomotion(s.raw)&&
  fresh(s.now,s.weapon_time)&&fresh(s.now,s.player_time);
}
inline uint8_t result(uint8_t original,uintptr_t return_rva,const State&s) {
 return original==1&&site(return_rva)>=0&&eligible(s)?0:original;
}
}
