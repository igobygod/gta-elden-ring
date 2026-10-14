#pragma once
#include <stdint.h>
namespace native_melee_policy {
constexpr uint32_t fists=110000, sword=2140000;
enum Mode : uint32_t { restore=0, unarmed=1, sword_and_flame=2 };
// The native unequip operation equips the built-in fist item. Never send an
// invalid inventory handle or synthetic item ID to EquipGear.
inline uint32_t restorable_item(uint32_t item){return item==0||item==0xFFFFFFFFu?fists:item;}
inline bool slots(uint32_t style,uint32_t left,uint32_t right,uint32_t& l,uint32_t& r){
 if(style>3||left>2||right>2)return false;l=left*2;r=right*2+1;return true;
}
struct Lease {uint32_t previous=0,assigned=0;bool owned=false;};
struct EquipmentLease {
 uintptr_t owner=0;Lease hands[6]={};uint32_t grant_attempts=0;
 void owner_changed(uintptr_t next){if(!next||owner==next)return;owner=next;for(auto&h:hands)h={};grant_attempts=0;}
 // Observing a manual equipment change gives ownership back to the game. A
 // later explicit melee selection can start a new lease from that new item.
 void observe(unsigned slot,uint32_t actual){if(slot<6&&hands[slot].owned&&actual!=hands[slot].assigned)hands[slot]={};}
 void applied(unsigned slot,uint32_t before,uint32_t item){if(slot>=6)return;observe(slot,before);auto&h=hands[slot];if(before==item)return;if(!h.owned)h.previous=before;h.assigned=item;h.owned=true;}
 bool restore_target(unsigned slot,uint32_t actual,uint32_t&target){if(slot>=6)return false;observe(slot,actual);auto&h=hands[slot];if(!h.owned)return false;target=restorable_item(h.previous);return true;}
 void restored(unsigned slot){if(slot<6)hands[slot]={};}
 bool pending()const{for(auto&h:hands)if(h.owned)return true;return false;}
};
}
