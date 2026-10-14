#pragma once
#include "cheat_console.h"
// Compact keyboard console using the original GTA font atlas.
static void draw_cheat_ui(Vertex*dst,size_t&count,unsigned width,unsigned height,const CheatConsoleSnapshot&s,uint64_t now){
 if(!s.active&&(!s.result[0]||now<s.result_time||now-s.result_time>4200))return;
 GtaHud h(dst,count,float(width),float(height));const float sc=fminf(width/1280.f,height/720.f),w=560*sc,x=-w*.5f,y=-float(height)*.5f+44*sc;
 auto clipped=[&](const char*text,float baseline,float size,bool tail){char row[160]={};size_t n=strlen(text),begin=0;float limit=w-32*sc;if(tail){while(begin<n&&h.length(text+begin,size)>limit)begin++;snprintf(row,sizeof(row),"%s",text+begin);}else{snprintf(row,sizeof(row),"%s",text);n=strlen(row);while(n&&h.length(row,size)>limit)row[--n]=0;}h.text(row,x+16*sc,baseline,size,1,1,1);};
 if(s.active){
  h.quad(x,y,w,72*sc,0,0,0,.83f);
  h.text("CHEAT CODE",x+16*sc,y+21*sc,13*sc,.72f,.72f,.72f);
  char display[100];snprintf(display,sizeof(display),"%s%s",s.input,((now/500)%2)?"_":"");
  clipped(s.pending?"Applying...":display,y+53*sc,22*sc,true);
 }else{
  h.quad(x,y,w,42*sc,0,0,0,.78f);
  clipped(s.result,y+28*sc,17*sc,false);
 }
}
