#pragma once
#include "../polish-fix/map/terrain_map.h"
#include "../polish-v4/radar/hud_atlas.h"
// Included after Vertex; GUI vertices use viewport-centred pixels.
struct GtaHud {
 Vertex*dst;size_t&count;float width,height;
 GtaHud(Vertex*d,size_t&n,float w,float h):dst(d),count(n),width(w),height(h){}
 void quad(float x,float y,float w,float h,float red,float green,float blue,float alpha,const gta_hud::Rect*tile=nullptr,float rotation=0){
  if(count+6>vertexCapacity||w<=0||h<=0)return;const float p[4][2]={{0,0},{1,0},{1,1},{0,1}};const int order[]={0,1,2,0,2,3};float cs=cosf(rotation),sn=sinf(rotation);
  for(int j:order){float xx=(p[j][0]-.5f)*w,yy=(p[j][1]-.5f)*h;Vertex v={(x+w*.5f+xx*cs-yy*sn)*2/width,-(y+h*.5f+xx*sn+yy*cs)*2/height,0,1,0,0};v.r=red;v.g=green;v.b=blue;v.a=alpha;v.frameBlend=tile?-9.f:-6.f;
   if(tile){v.u=(tile->x+p[j][0]*tile->w)/1024.f;v.v=(tile->y+p[j][1]*tile->h)/1024.f;}dst[count++]=v;}
 }
 void icon(const char*name,float x,float y,float w,float h,float r=1,float g=1,float b=1,float a=1,float rotation=0){auto rect=gta_hud::find(name);if(rect)quad(x,y,w,h,r,g,b,a,rect,rotation);}
 float length(const char*s,float size){float n=0;for(;*s;s++)if(auto g=gta_hud::glyph((unsigned char)*s))n+=g->advance*size/32.f;return n;}
 void text(const char*s,float x,float baseline,float size,float r=1,float g=1,float b=1,float a=1,bool centered=false){
  float scale=size/32.f;if(centered)x-=length(s,size)*.5f;
  for(;*s;s++){auto glyph=gta_hud::glyph((unsigned char)*s);if(!glyph)continue;auto&rect=glyph->rect;quad(x+glyph->bearingX*scale+1,baseline+glyph->bearingY*scale+1,rect.w*scale,rect.h*scale,0,0,0,a*.75f,&rect);quad(x+glyph->bearingX*scale,baseline+glyph->bearingY*scale,rect.w*scale,rect.h*scale,r,g,b,a,&rect);x+=glyph->advance*scale;}
 }
 void bar(float x,float y,float w,float value,float maximum,float red,float green,float blue){
  float h=7*height/720.f,f=maximum>0?fmaxf(0.f,fminf(1.f,value/maximum)):0;
  quad(x-1,y-1,w+2,h+2,0,0,0,.95f);quad(x,y,w,h,red*.2f,green*.2f,blue*.2f,.85f);quad(x,y,w*f,h,red,green,blue,.95f);
 }
};
static void draw_gta_hud(Vertex*dst,size_t&count,unsigned width,unsigned height,const PlayerStatus&p,const RifleState&s,bool fresh,const float*cam,const float*bodyFacing,uint64_t now){
 if(!p.valid)return;GtaHud h(dst,count,float(width),float(height));float sc=fminf(width/1280.f,height/720.f);float left=-float(width)*.5f+28*sc,bottom=height*.5f-29*sc,mapW=224*sc,mapH=144*sc,top=bottom-mapH-17*sc,cx=left+mapW*.5f,cy=top+mapH*.5f;
 h.quad(left-2*sc,top-2*sc,mapW+4*sc,mapH+4*sc,0,0,0,.9f);h.quad(left,top,mapW,mapH,.10f,.13f,.14f,.94f);
 float cameraYaw=atan2f(cam[8],cam[10]),ppm=mapW/160.f;
 auto mapUV=p.map_valid?er_terrain_map::uv(p.map_block_id,p.map_local_position):er_terrain_map::UV{};
 if(mapUV.valid&&count+6<=vertexCapacity){
  const float corners[4][2]={{0,0},{1,0},{1,1},{0,1}};const int order[]={0,1,2,0,2,3};float cs=cosf(cameraYaw),sn=sinf(cameraYaw);
  for(int j:order){float sx=(corners[j][0]-.5f)*mapW/ppm,sy=(corners[j][1]-.5f)*mapH/ppm,dx=sx*cs-sy*sn,dz=-sx*sn-sy*cs;
   Vertex v={(left+corners[j][0]*mapW)*2/width,-(top+corners[j][1]*mapH)*2/height,0,1,mapUV.u+er_terrain_map::uv_radius(dx),mapUV.v-er_terrain_map::uv_radius(dz)};v.frameBlend=-10;dst[count++]=v;
  }
 }else for(int i=1;i<4;i++){h.quad(left+mapW*i/4,top,sc,mapH,.25f,.30f,.30f,.32f);h.quad(left,top+mapH*i/4,mapW,sc,.25f,.30f,.30f,.32f);}
 h.text(mapUV.valid?"LIMGRAVE  80m":"RADAR  80m",left+7*sc,top+14*sc,12*sc,.92f,.95f,.95f);
 if(now>=p.radar_time&&now-p.radar_time<650)for(unsigned i=0;i<p.hostile_count&&i<32;i++){
  const auto&e=p.hostiles[i];float dx=e.position[0]-p.position[0],dz=e.position[2]-p.position[2];
  float x=cx+(dx*cosf(cameraYaw)-dz*sinf(cameraYaw))*ppm,y=cy-(dx*sinf(cameraYaw)+dz*cosf(cameraYaw))*ppm;
  if(fabsf(x-cx)>=mapW*.46f||fabsf(y-cy)>=mapH*.43f)continue;
  const char*icon="enemy_blip";float size=8*sc,r=.92f,g=.12f,b=.12f;
  if(e.category==3){icon="radar_animal";size=13*sc;r=(e.flags&1)?.98f:.94f;g=(e.flags&1)?.18f:.77f;b=(e.flags&1)?.12f:.37f;}
  else if(e.category==2||e.category==4){icon="radar_person";size=12*sc;r=.38f;g=e.category==4?.94f:.76f;b=e.category==4?.48f:1.f;}
  else if(e.category!=1){h.quad(x-4*sc,y-4*sc,8*sc,8*sc,0,0,0,.9f,nullptr,.785398f);h.quad(x-2.5f*sc,y-2.5f*sc,5*sc,5*sc,.76f,.78f,.79f,.95f,nullptr,.785398f);continue;}
  h.icon(icon,x-size*.5f-sc,y-size*.5f-sc,size+2*sc,size+2*sc,0,0,0,.9f);
  h.icon(icon,x-size*.5f,y-size*.5f,size,size,r,g,b,1.f);
 }
 h.icon("north",left+mapW-18*sc,top+5*sc,12*sc,12*sc,1,1,1,1,-cameraYaw);float arrowYaw=atan2f(-bodyFacing[0],-bodyFacing[1])-cameraYaw;
 h.icon("player_arrow_stroke",cx-7*sc,cy-8.5f*sc,14*sc,17*sc,1,1,1,1,arrowYaw);h.icon("player_arrow",cx-7*sc,cy-8.5f*sc,14*sc,17*sc,1,1,1,1,arrowYaw);
 h.bar(left,bottom-9*sc,mapW*.49f,float(p.hp),float(p.max_hp),.35f,.72f,.43f);
 h.bar(left+mapW*.51f,bottom-9*sc,mapW*.23f,float(p.fp),float(p.max_fp),.32f,.60f,.88f);
 h.bar(left+mapW*.76f,bottom-9*sc,mapW*.24f,float(p.stamina),float(p.max_stamina),.93f,.78f,.27f);
 char line[96];snprintf(line,sizeof(line),"HP %d/%d    FP %d    ST %d",p.hp,p.max_hp,p.fp,p.stamina);h.text(line,left,bottom+15*sc,12*sc,1,1,1);
 h.text(p.invincible?"INVINCIBLE   ~ CHEATS":"~  CHEATS",left,top-8*sc,13*sc,.85f,.85f,.85f);
 if(s.no_reload)h.text("NORELOAD",left,top-25*sc,13*sc,.94f,.8f,.38f);

 if(fresh&&s.weapon==0&&!s.wheel_open){const char*name=s.melee_mode==2?"SWORD OF NIGHT AND FLAME":s.melee_mode==1?"UNARMED":"EQUIPPING MELEE";float x=width*.5f-28*sc;h.text(name,x-h.length(name,14*sc),-float(height)*.5f+78*sc,14*sc);}

 if(fresh&&s.weapon&&s.weapon<8&&!s.wheel_open){const char* names[]={"UNARMED","PISTOL","SMG","SHOTGUN","ASSAULT RIFLE","SNIPER","GRENADE LAUNCHER","GRENADE"};names[6]=s.launcher_variant?"RPG":"GRENADE LAUNCHER";const char*icons[]={"","weapon_pistol","weapon_smg","weapon_shotgun","weapon_rifle","weapon_sniper","weapon_launcher","weapon_grenade"};float x=width*.5f-28*sc;
  h.icon(icons[s.weapon],x-48*sc,-float(height)*.5f+28*sc,46*sc,31*sc);snprintf(line,sizeof(line),"%u / %u",s.ammo,s.reserve);h.text(line,x-60*sc-h.length(line,25*sc),-float(height)*.5f+53*sc,25*sc);
  h.text(names[s.weapon],x-h.length(names[s.weapon],14*sc),-float(height)*.5f+78*sc,14*sc);
  if(s.reload_start&&now>=s.reload_start){const float times[]={1,1833,2000,2600,2033,2867,2867,2400};float t=fminf(1.f,float(now-s.reload_start)/(s.weapon==6&&s.launcher_variant?2733.f:times[s.weapon]));h.text("RELOADING",x-120*sc,-float(height)*.5f+99*sc,14*sc);h.bar(x-120*sc,-float(height)*.5f+108*sc,120*sc,t,1,.9f,.9f,.9f);}
 }
}
