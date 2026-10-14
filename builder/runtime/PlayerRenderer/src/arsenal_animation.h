#pragma once
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include "directional_motion.h"
namespace arsenal {
struct V {float x,y,z;};
using Q=donor_motion::Q;
struct M {float v[16];};
inline Q norm(Q q){float d=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);if(d<1e-8f)return {0,0,0,1};return {q.x/d,q.y/d,q.z/d,q.w/d};}
inline Q mix(Q a,Q b,float t){float sign=a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w<0?-1.f:1.f;return norm({a.x+(b.x*sign-a.x)*t,a.y+(b.y*sign-a.y)*t,a.z+(b.z*sign-a.z)*t,a.w+(b.w*sign-a.w)*t});}
inline V mix(V a,V b,float t){return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};}
inline M mul(const M&a,const M&b){M r={};for(int i=0;i<4;i++)for(int j=0;j<4;j++)for(int k=0;k<4;k++)r.v[i*4+j]+=a.v[i*4+k]*b.v[k*4+j];return r;}
inline V point(const M&m,V p){return {m.v[0]*p.x+m.v[1]*p.y+m.v[2]*p.z+m.v[3],m.v[4]*p.x+m.v[5]*p.y+m.v[6]*p.z+m.v[7],m.v[8]*p.x+m.v[9]*p.y+m.v[10]*p.z+m.v[11]};}
inline bool invert(const M&m,M&r){float a=m.v[0],b=m.v[1],c=m.v[2],d=m.v[4],e=m.v[5],f=m.v[6],g=m.v[8],h=m.v[9],i=m.v[10];float det=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);if(!std::isfinite(det)||std::fabs(det)<1e-9f)return false;r={};r.v[0]=(e*i-f*h)/det;r.v[1]=(c*h-b*i)/det;r.v[2]=(b*f-c*e)/det;r.v[4]=(f*g-d*i)/det;r.v[5]=(a*i-c*g)/det;r.v[6]=(c*d-a*f)/det;r.v[8]=(d*h-e*g)/det;r.v[9]=(b*g-a*h)/det;r.v[10]=(a*e-b*d)/det;V t=point(r,{m.v[3],m.v[7],m.v[11]});r.v[3]=-t.x;r.v[7]=-t.y;r.v[11]=-t.z;r.v[15]=1;return true;}
inline M trs(V t,Q q,V scale){q=norm(q);float x=q.x,y=q.y,z=q.z,w=q.w;M r={{(1-2*y*y-2*z*z)*scale.x,(2*x*y-2*z*w)*scale.y,(2*x*z+2*y*w)*scale.z,t.x,(2*x*y+2*z*w)*scale.x,(1-2*x*x-2*z*z)*scale.y,(2*y*z-2*x*w)*scale.z,t.y,(2*x*z-2*y*w)*scale.x,(2*y*z+2*x*w)*scale.y,(1-2*x*x-2*y*y)*scale.z,t.z,0,0,0,1}};return r;}
struct Pose {std::vector<V> t;std::vector<Q> q;};
struct Clip {uint32_t slot,action,frames,flags;float duration;std::vector<V> t;std::vector<Q> q;};
struct Mesh {uint32_t flags=0;M attachment={};V muzzle={};std::vector<V> p;std::vector<float> uv;std::vector<uint32_t> indices;};
struct SkinJ {uint32_t j[4];};struct SkinW {float w[4];};
class Rig {
public:
 unsigned bones=0,nv=0,hand=0;std::vector<int32_t> parent;std::vector<M> rest,inv,world,skinMatrices;std::vector<V> scale,body;std::vector<SkinJ> joints;std::vector<SkinW> weights;std::vector<Clip> clips;Mesh mesh[8];Mesh projectile_mesh;std::vector<bool> upper,lower;
 template<class T> static bool read(FILE*f,T*p,size_t n){return fread(p,sizeof(T),n,f)==n;}
 template<class T> static bool readv(FILE*f,std::vector<T>&v,size_t n){v.resize(n);return read(f,v.data(),n);}
 static bool finite(V p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
 static bool finite(const M&m){for(float x:m.v)if(!std::isfinite(x))return false;return true;}
 static bool quaternion(Q q){double d=double(q.x)*q.x+double(q.y)*q.y+double(q.z)*q.z+double(q.w)*q.w;return std::isfinite(d)&&std::fabs(d-1.)<0.01;}
 static bool affine(const M&m){return finite(m)&&std::fabs(m.v[12])<1e-6f&&std::fabs(m.v[13])<1e-6f&&std::fabs(m.v[14])<1e-6f&&std::fabs(m.v[15]-1)<1e-6f;}
 static bool readMesh(FILE*f,const unsigned*a,Mesh&w){
  if(a[1]>=100000||a[2]>=300000||a[2]%3)return false;w.flags=a[3];
  if(!read(f,&w.attachment,1)||!read(f,&w.muzzle,1)||!readv(f,w.p,a[1])||!readv(f,w.uv,size_t(a[1])*2)||!readv(f,w.indices,a[2]))return false;
  if(!affine(w.attachment)||!finite(w.muzzle))return false;
  for(auto p:w.p)if(!finite(p))return false;for(float u:w.uv)if(!std::isfinite(u))return false;for(auto idx:w.indices)if(idx>=w.p.size())return false;return true;
 }
 bool loadWeapon(const char*path,unsigned slot){if(slot>=8)return false;FILE*f=igobygod::open(path,"rb");if(!f)return false;unsigned h[4],a[4];Mesh m;bool ok=read(f,h,4)&&h[0]==0x414d5348&&h[1]==1&&h[2]==1&&h[3]==4096&&read(f,a,4)&&a[0]==slot&&readMesh(f,a,m)&&fgetc(f)==EOF;fclose(f);if(ok)mesh[slot]=std::move(m);return ok;}
 bool loadProjectile(const char*path){FILE*f=igobygod::open(path,"rb");if(!f)return false;unsigned h[4],a[4];Mesh m;
  bool ok=read(f,h,4)&&h[0]==0x414d5348&&h[1]==1&&h[2]==1&&h[3]==4096&&read(f,a,4)&&a[0]==6&&readMesh(f,a,m)&&fgetc(f)==EOF;fclose(f);if(ok)projectile_mesh=std::move(m);return ok;
 }
 bool load(const char* jp,const char*mp,unsigned expected){FILE*f=igobygod::open(jp,"rb");if(!f)return false;unsigned h[6];bool ok=read(f,h,6)&&h[0]==0x414a4e54&&h[1]==1&&h[2]>45&&h[2]<512&&h[3]>0&&h[3]<=100000&&h[3]==expected&&h[4]>0&&h[4]<=128&&h[5]<h[2];if(!ok){fclose(f);return false;}bones=h[2];nv=h[3];hand=h[5];ok=readv(f,parent,bones)&&readv(f,rest,bones)&&readv(f,body,nv)&&readv(f,joints,nv)&&readv(f,weights,nv);clips.resize(h[4]);for(auto&c:clips){if(!ok)break;unsigned v[4];ok=read(f,v,4)&&read(f,&c.duration,1)&&v[0]<8&&v[1]<16&&v[2]>=1&&v[2]<=480&&std::isfinite(c.duration)&&c.duration>0&&c.duration<30;if(!ok)break;c.slot=v[0];c.action=v[1];c.frames=v[2];c.flags=v[3];ok=readv(f,c.t,size_t(bones)*c.frames)&&readv(f,c.q,size_t(bones)*c.frames);for(auto x:c.t)ok=ok&&finite(x);for(auto q:c.q)ok=ok&&quaternion(q);}
 ok=ok&&fgetc(f)==EOF;fclose(f);if(!ok)return false;
 world.resize(bones);inv.resize(bones);scale.resize(bones);upper.assign(bones,false);lower.assign(bones,false);
 for(unsigned j=0;j<bones;j++){if(parent[j]>=int(j)||parent[j]<-1||!affine(rest[j]))return false;world[j]=parent[j]<0?rest[j]:mul(world[parent[j]],rest[j]);if(!finite(world[j])||!invert(world[j],inv[j])||!finite(inv[j]))return false;for(int k=0;k<3;k++){float magnitude=0;for(int a=0;a<3;a++)magnitude+=rest[j].v[a*4+k]*rest[j].v[a*4+k];float value=std::sqrt(magnitude);if(!std::isfinite(value)||value<1e-8f)return false;(&scale[j].x)[k]=value;}for(int k=int(j);k>=0;k=parent[k]){if(k==45)upper[j]=true;if(k==1)lower[j]=true;}}
 for(unsigned v=0;v<nv;v++){if(!finite(body[v]))return false;float sum=0;for(int a=0;a<4;a++){if(joints[v].j[a]>=bones||!std::isfinite(weights[v].w[a])||weights[v].w[a]<0||weights[v].w[a]>1.01f)return false;sum+=weights[v].w[a];}if(!std::isfinite(sum)||std::fabs(sum-1)>0.01f)return false;}
 f=igobygod::open(mp,"rb");if(!f)return false;unsigned m[4];ok=read(f,m,4)&&m[0]==0x414d5348&&m[1]==1&&m[2]==8&&m[3]==4096;bool seen[8]={};for(int k=0;k<8&&ok;k++){unsigned a[4];ok=read(f,a,4)&&a[0]<8&&!seen[a[0]];if(!ok)break;seen[a[0]]=true;ok=readMesh(f,a,mesh[a[0]]);}
 ok=ok&&fgetc(f)==EOF;fclose(f);return ok;
 }
 const Clip* find(unsigned slot,unsigned action)const{for(auto&c:clips)if(c.slot==slot&&c.action==action)return &c;return nullptr;}
 void sample(const Clip*c,float time,bool loop,Pose&out)const{out.t.resize(bones);out.q.resize(bones);if(!c)return;float p=std::max(0.f,time/c->duration);if(loop)p-=std::floor(p);else p=std::min(p,1.f);float frame=p*(c->frames-1);unsigned a=std::min(unsigned(frame),c->frames-1),b=loop?(a+1)%c->frames:std::min(a+1,c->frames-1);float u=frame-std::floor(frame);for(unsigned j=0;j<bones;j++){out.t[j]=mix(c->t[size_t(a)*bones+j],c->t[size_t(b)*bones+j],u);out.q[j]=mix(c->q[size_t(a)*bones+j],c->q[size_t(b)*bones+j],u);}}
 void blend(Pose&to,const Pose&b,float t)const{for(unsigned j=0;j<bones;j++){to.t[j]=mix(to.t[j],b.t[j],t);to.q[j]=mix(to.q[j],b.q[j],t);}}
 void matrices(const Pose&p){for(unsigned j=0;j<bones;j++){M local=trs(p.t[j],p.q[j],scale[j]);world[j]=parent[j]<0?local:mul(world[parent[j]],local);}}
 void skin(std::vector<float>&out){skinMatrices.resize(bones);for(unsigned j=0;j<bones;j++)skinMatrices[j]=mul(world[j],inv[j]);out.resize(size_t(nv)*3);for(unsigned v=0;v<nv;v++){V p={};for(int k=0;k<4;k++){float w=weights[v].w[k];if(w==0)continue;V a=point(skinMatrices[joints[v].j[k]],body[v]);p.x+=a.x*w;p.y+=a.y*w;p.z+=a.z*w;}out[v*3]=p.x;out[v*3+1]=p.y;out[v*3+2]=p.z;}}
 size_t maxMeshIndices()const{size_t n=0;for(auto&m:mesh)n=std::max(n,m.indices.size());return n;}
};
}
