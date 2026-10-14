#pragma once
#include <array>
#include <vector>
#include <fstream>
#include <filesystem>
#include <string>
#include <cstdint>
#include <cmath>
#include <algorithm>
namespace carasset {
struct Vertex { float p[3], n[3], uv[2]; };
struct Range { uint32_t material, first, count; };
struct Material { uint32_t id, kind; float rect[4], tint[4]; }; // 0 opaque,1 paint,2 cutout/blended light,3 glass,4 omitted display
struct Mesh { std::string name; std::array<float,3> pivot{}; std::vector<Vertex> vertices; std::vector<uint32_t> indices; std::vector<Range> ranges; };
struct Asset {
 uint32_t atlasSize=0; std::array<float,6> bounds{}; std::array<float,3> driverSeat{};std::vector<Material> materials;std::vector<Mesh> meshes;
 bool load(const char* path) {
  Asset a;std::ifstream f(std::filesystem::path(igobygod::wide(path)),std::ios::binary);if(!f)return false;
  auto read=[&](void*p,size_t n){return bool(f.read(static_cast<char*>(p),std::streamsize(n)));};
  uint32_t h[6];if(!read(h,sizeof h)||h[0]!=0x42524143||h[1]!=1||h[2]!=5||h[3]>16||h[4]!=2048)return false;
  if(!read(a.bounds.data(),24)||!read(a.driverSeat.data(),12))return false;
  for(float v:a.bounds)if(!std::isfinite(v))return false;
  for(float v:a.driverSeat)if(!std::isfinite(v))return false;
  for(int i=0;i<3;++i)if(a.bounds[i]>a.bounds[i+3])return false;
  a.atlasSize=h[4];a.materials.resize(h[3]);if(!read(a.materials.data(),a.materials.size()*sizeof(Material)))return false;
  for(size_t i=0;i<a.materials.size();++i){auto&m=a.materials[i];if(m.id!=i||m.kind>4)return false;for(float v:m.rect)if(!std::isfinite(v)||v<0||v>1)return false;for(float v:m.tint)if(!std::isfinite(v)||v<0||v>1)return false;if(m.rect[0]+m.rect[2]>1||m.rect[1]+m.rect[3]>1)return false;}
  a.meshes.resize(h[2]);uint32_t totalV=0,totalI=0;
  for(auto&m:a.meshes){char name[32];uint32_t n[3];if(!read(name,32)||!read(m.pivot.data(),12)||!read(n,12))return false;for(float v:m.pivot)if(!std::isfinite(v))return false;
   if(n[0]>30000||n[1]>200000||n[2]>64||n[1]%3||!n[0]||!n[1])return false;
   totalV+=n[0];totalI+=n[1];if(totalV>50000||totalI>250000)return false;
   m.name.assign(name,std::find(name,name+32,'\0'));m.vertices.resize(n[0]);m.indices.resize(n[1]);m.ranges.resize(n[2]);
   if(!read(m.vertices.data(),m.vertices.size()*sizeof(Vertex))||!read(m.indices.data(),m.indices.size()*4)||!read(m.ranges.data(),m.ranges.size()*sizeof(Range)))return false;
   for(auto&v:m.vertices){for(float t:v.p)if(!std::isfinite(t)||std::abs(t)>10)return false;for(float t:v.n)if(!std::isfinite(t))return false;for(float t:v.uv)if(!std::isfinite(t))return false;float n2=v.n[0]*v.n[0]+v.n[1]*v.n[1]+v.n[2]*v.n[2];if(n2<.98f||n2>1.02f)return false;}
   for(uint32_t i:m.indices)if(i>=n[0])return false;
   uint32_t cursor=0;for(auto&r:m.ranges){if(r.material>=h[3]||r.first!=cursor||r.count>n[1]-cursor||r.count%3)return false;cursor+=r.count;}if(cursor!=n[1])return false;
  }
  if(f.peek()!=std::char_traits<char>::eof())return false;*this=std::move(a);return true;
 }
};
static_assert(sizeof(Vertex)==32&&sizeof(Range)==12&&sizeof(Material)==40,"CARB packed fields require 32-bit floats/integers");
} // namespace carasset
