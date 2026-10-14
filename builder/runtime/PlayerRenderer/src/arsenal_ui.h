#pragma once
#include "arsenal_wheel_policy.h"
#include "../polish-v4.3/camera-wheel/wheel_atlas.h"
// GTA's wheel artwork is read from the local weapons_all HUD sheet. Eight
// gameplay IDs remain stable; only their screen positions follow GTA categories.
static void draw_arsenal_ui(Vertex*dst,size_t&count,const RifleState&s,bool fresh,unsigned width,unsigned height){
 if(!fresh||!s.wheel_open||!width||!height)return;
 GtaHud h(dst,count,float(width),float(height));float scale=fminf(width/1280.f,height/720.f);
 auto vertex=[&](float x,float y,float r,float g,float b,float a){if(count>=vertexCapacity)return;Vertex v={x*2.f/width,-y*2.f/height,0,1,0,0};v.r=r;v.g=g;v.b=b;v.a=a;v.frameBlend=-6;dst[count++]=v;};
 auto arc=[&](float centre,float inner,float outer,float r,float g,float blue,float alpha){
  for(unsigned part=0;part<32;part++){float a=centre-.387f+part*(.774f/32),b=centre-.387f+(part+1)*(.774f/32);float p[4][2]={{sinf(a)*inner,-cosf(a)*inner},{sinf(a)*outer,-cosf(a)*outer},{sinf(b)*outer,-cosf(b)*outer},{sinf(b)*inner,-cosf(b)*inner}};for(int i:{0,1,2,0,2,3})vertex(p[i][0],p[i][1],r,g,blue,alpha);}
 };
 // Original HUD_WEAPON_WHEEL.as placement in its 1280x720 coordinate space.
 constexpr float centres[8][2]={{0,-170},{116,-112},{162,0},{116,112},{0,162},{-116,112},{-162,0},{-116,-112}};
 const char*names[]={s.melee_variant?"Sword of Night and Flame":"Unarmed","Pistol","SMG","Pump Shotgun","Assault Rifle","Sniper Rifle",s.launcher_variant?"RPG":"Grenade Launcher","Grenade"};
 h.quad(-float(width)*.5f,-float(height)*.5f,float(width),float(height),.04f,.07f,.05f,.19f);
 for(unsigned sector=0;sector<8;sector++){
  unsigned slot=arsenal_wheel::weapon_at_sector(sector);bool hover=slot==s.wheel_hover,equipped=slot==s.weapon;float angle=sector*.7853981634f;
  arc(angle,99*scale,239*scale,0,0,0,.80f);
  arc(angle,102*scale,236*scale,hover?.85f:.12f,hover?.85f:.12f,hover?.85f:.12f,hover?.77f:.70f);
  if(equipped)arc(angle,235*scale,241*scale,.45f,.70f,.36f,1.f);
  auto index=gta_wheel_art::index(slot,s.launcher_variant!=0,s.melee_variant!=0);auto&art=gta_wheel_art::icons[index];float iw=gta_wheel_art::sizes[index][0]*scale,ih=gta_wheel_art::sizes[index][1]*scale;
  float x=centres[sector][0]*scale,y=centres[sector][1]*scale;
  h.quad(x-iw*.5f,y-ih*.5f,iw,ih,1,1,1,1,&art);
  char hotkey[2]={char(slot?'0'+slot:'8'),0};h.text(hotkey,x,y+50*scale,12*scale,1,1,1,.8f,true);
  if(equipped&&slot){char ammo[48];snprintf(ammo,sizeof(ammo),"%u / %u",s.ammo,s.reserve);h.text(ammo,x,y+34*scale,16*scale,1,1,1,1,true);}
 }
 unsigned hovered=s.wheel_hover<8?s.wheel_hover:0;
 if(hovered==0&&s.melee_variant){h.text("Sword of Night",0,-15*scale,17*scale,1,1,1,1,true);h.text("and Flame",0,4*scale,17*scale,1,1,1,1,true);}
 else h.text(names[hovered],0,-5*scale,18*scale,1,1,1,1,true);
 if(hovered==6||hovered==0){char page[16];snprintf(page,sizeof(page),"<  %u / 2  >",(hovered==0?s.melee_variant:s.launcher_variant)?2:1);h.text(page,0,26*scale,15*scale,1,1,1,1,true);}
 h.text(hovered==6||hovered==0?"Scroll to switch":"Release Tab to equip",0,52*scale,12*scale,.85f,.85f,.85f,1,true);
}
