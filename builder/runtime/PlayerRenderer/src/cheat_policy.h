#pragma once
#include <cstring>
#include <cctype>
#include <cstdlib>
#include "cheat_console.h"
#include "../polish-next/player-status/health_policy.h"
namespace cheat_policy {
inline int health_value(int hp,int maximum,int target,bool fill){int scaled=status_health::scaled(hp,maximum,target);if(scaled<0||hp==0)return scaled;return fill?target:scaled;}
inline bool parse(const char*text,CheatCommand&out){
 char b[96];size_t n=std::strlen(text);if(n>=sizeof(b))return false;for(size_t i=0;i<=n;i++)b[i]=(char)std::toupper((unsigned char)text[i]);
 char*p=b;while(*p==' ')p++;char*arg=std::strchr(p,' ');if(arg){*arg++=0;while(*arg==' ')arg++;}out={};
 if(!std::strcmp(p,"MAXHEALTH")||!std::strcmp(p,"TURTLE")){out.kind=1;if(!arg||!*arg){out.value=2000;return true;}if(!std::strcmp(arg,"RESTORE")){out.value=0;return true;}char*end=nullptr;long v=std::strtol(arg,&end,10);while(end&&*end==' ')end++;if(!end||*end||v<10||v>50000)return false;out.value=(int)v;return true;}
 if(arg&&*arg)return false;
 if(!std::strcmp(p,"INVINCIBLE")||!std::strcmp(p,"PAINKILLER"))out.kind=2;
 else if(!std::strcmp(p,"MAXAMMO")||!std::strcmp(p,"TOOLUP"))out.kind=3;
 else if(!std::strcmp(p,"NORELOAD"))out.kind=4;
 else if(!std::strcmp(p,"BUFFALO")||!std::strcmp(p,"CAR"))out.kind=5;
 else if(!std::strcmp(p,"CARCLEAR"))out.kind=6;
 return out.kind!=0;
}
}
