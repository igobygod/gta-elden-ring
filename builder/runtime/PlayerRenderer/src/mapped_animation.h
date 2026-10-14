#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cmath>
#include <vector>
#include <limits>
#ifdef _WIN32
#include <windows.h>
#endif
namespace mapped_animation {
template<class T>struct Span {const T* ptr=nullptr;size_t count=0;const T* data()const{return ptr;}size_t size()const{return count;}bool empty()const{return count==0;}const T&operator[](size_t i)const{return ptr[i];}const T*begin()const{return ptr;}const T*end()const{return count?ptr+count:ptr;}};
struct Cursor {const uint8_t* bytes=nullptr;size_t size=0,offset=0;bool take(size_t count,size_t unit,const void*&p){if(unit&&count>(size-offset)/unit)return false;size_t n=count*unit;p=bytes+offset;offset+=n;return true;}template<class T>bool value(T&v){const void*p;if(!take(1,sizeof(T),p))return false;std::memcpy(&v,p,sizeof(T));return true;}template<class T>bool span(size_t n,Span<T>&s){const void*p;if(!take(n,sizeof(T),p)||uintptr_t(p)%alignof(T))return false;s={static_cast<const T*>(p),n};return true;}};
class Mapping {
#ifdef _WIN32
 HANDLE file=INVALID_HANDLE_VALUE,map=nullptr;
#endif
 const uint8_t* p=nullptr;size_t n=0;
public:
 Mapping()=default;Mapping(const Mapping&)=delete;Mapping&operator=(const Mapping&)=delete;~Mapping(){close();}
 void close(){
#ifdef _WIN32
 if(p)UnmapViewOfFile(p);if(map)CloseHandle(map);if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);file=INVALID_HANDLE_VALUE;map=nullptr;
#endif
 p=nullptr;n=0;}
 bool open(const char*path){close();
#ifdef _WIN32
 file=CreateFileW(igobygod::wide(path).c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_RANDOM_ACCESS,nullptr);if(file==INVALID_HANDLE_VALUE)return false;LARGE_INTEGER size={};if(!GetFileSizeEx(file,&size)||size.QuadPart<=0||uint64_t(size.QuadPart)>uint64_t(SIZE_MAX)){close();return false;}n=size_t(size.QuadPart);map=CreateFileMappingA(file,nullptr,PAGE_READONLY,0,0,nullptr);if(!map){close();return false;}p=(const uint8_t*)MapViewOfFile(map,FILE_MAP_READ,0,0,0);if(!p){close();return false;}return true;
#else
 (void)path;return false;
#endif
 }
 const uint8_t*data()const{return p;}size_t size()const{return n;}
};
struct ModelView {uint32_t nv=0,nf=0,nc=0;Span<uint32_t>indices;Span<float>uvs,poses;float durations[4]={};};
inline bool parseModel(const void*data,size_t size,ModelView&out){out={};if(!data||size<20)return false;Cursor c{static_cast<const uint8_t*>(data),size};uint32_t h[5];for(auto&v:h)if(!c.value(v))return false;if(h[0]!=0x47544151||!h[1]||h[1]>100000||!h[2]||h[2]>1000000||h[3]!=120||h[4]!=4)return false;ModelView m;m.nv=h[1];m.nf=h[3];m.nc=h[4];if(!c.span(h[2],m.indices)||!c.span(size_t(m.nv)*2,m.uvs))return false;for(auto&v:m.durations)if(!c.value(v)||!std::isfinite(v)||v<=0)return false;if(!c.span(size_t(m.nv)*3*m.nf*m.nc,m.poses)||c.offset!=size)return false;
 // Index buffer is small and needed immediately; do not scan/touch pose floats.
 for(auto i:m.indices)if(i>=m.nv)return false;out=m;return true;}
struct Clip {uint32_t id=0,frames=0;float duration=0;Span<float>vertices;};
inline bool parseRoll(const void*data,size_t size,uint32_t nv,std::vector<Clip>&out){out.clear();if(!data||!nv||nv>100000||size<12)return false;Cursor c{static_cast<const uint8_t*>(data),size};uint32_t magic=0,count=0,fileNv=0;if(!c.value(magic)||!c.value(count)||!c.value(fileNv)||magic!=0x524f4c4c||count>64||fileNv!=nv)return false;std::vector<Clip> clips;clips.reserve(count);for(uint32_t i=0;i<count;i++){Clip x;if(!c.value(x.id)||!c.value(x.frames)||!c.value(x.duration)||x.frames<2||x.frames>240||!std::isfinite(x.duration)||x.duration<=0||!c.span(size_t(x.frames)*nv*3,x.vertices))return false;clips.push_back(x);}if(c.offset!=size)return false;out.swap(clips);return true;}
struct Model {Mapping mapping;ModelView view;bool load(const char*path){view={};if(!mapping.open(path))return false;if(!parseModel(mapping.data(),mapping.size(),view)){mapping.close();return false;}return true;}};
struct Rolls {Mapping mapping;std::vector<Clip>clips;bool load(const char*path,uint32_t nv){clips.clear();if(!mapping.open(path))return false;if(!parseRoll(mapping.data(),mapping.size(),nv,clips)){mapping.close();return false;}return true;}};
// Keep Model/Rolls owners alive as long as views/spans are consumed. All views are
// const: original float bits remain untouched, and pages are faulted on demand.
// No full pose finite scan atload; rendering should reject nonfinite sampled poses.
}
