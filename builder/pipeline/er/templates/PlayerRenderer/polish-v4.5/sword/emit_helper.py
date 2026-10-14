import os
from build_carry import *
import hashlib
lines=['#pragma once','#include "../../src/arsenal_animation.h"','namespace sword_carry {','// Sword fallback only; never apply to a sampled native attack.', 'struct Joint {unsigned id; arsenal::Q q; float weight;};','constexpr Joint joints[] = {']
for i,q in zip(carry_ids,carry_q):
 weight=.85 if names[i]=='SKEL_R_UpperArm'else 1.
 lines.append(' {%d,{%s},%.8ff}, // %s'%(i,','.join('%.9ff'%float(v)for v in q),weight,names[i]))
lines+=['};','inline arsenal::M attachment(){return arsenal::M{{'+','.join('%.9ff'%float(x)for x in attachment.flatten())+'}};}','inline bool apply(arsenal::Pose& p, const arsenal::Rig& rig){',' if(rig.bones!=317||rig.hand!=106||p.t.size()!=317||p.q.size()!=317)return false;',' for(auto& j:joints)p.q[j.id]=arsenal::mix(p.q[j.id],j.q,j.weight);',' return true;','}','}']
(O/'sword_carry.h').write_text('\n'.join(lines)+'\n')
print(O/'sword_carry.h')
s=(O/'sword_carry.h').read_text().rsplit('}',1)[0]
s+=r'''
// Reflections cannot be interpolated as raw columns through a 180-degree turn.
// Remove the third-axis reflection, slerp the proper rotation, restore it.
inline float determinant(const arsenal::M&m){return m.v[0]*(m.v[5]*m.v[10]-m.v[6]*m.v[9])-m.v[1]*(m.v[4]*m.v[10]-m.v[6]*m.v[8])+m.v[2]*(m.v[4]*m.v[9]-m.v[5]*m.v[8]);}
inline arsenal::Q rotation(const arsenal::M&m,float handed){
 float a=m.v[0],b=m.v[1],c=m.v[2]*handed,d=m.v[4],e=m.v[5],f=m.v[6]*handed,g=m.v[8],h=m.v[9],i=m.v[10]*handed;
 arsenal::Q q={};float tr=a+e+i,s;
 if(tr>0){s=2*std::sqrt(std::max(0.f,tr+1));q={(h-f)/s,(c-g)/s,(d-b)/s,s*.25f};}
 else if(a>e&&a>i){s=2*std::sqrt(std::max(0.f,1+a-e-i));q={s*.25f,(b+d)/s,(c+g)/s,(h-f)/s};}
 else if(e>i){s=2*std::sqrt(std::max(0.f,1+e-a-i));q={(b+d)/s,s*.25f,(f+h)/s,(c-g)/s};}
 else{s=2*std::sqrt(std::max(0.f,1+i-a-e));q={(c+g)/s,(f+h)/s,s*.25f,(d-b)/s};}
 return arsenal::norm(q);
}
inline arsenal::M blendAttachment(const arsenal::M&a,const arsenal::M&b,float u){
 if(u<=0)return a;if(u>=1)return b;
 float handed=determinant(a)<0?-1.f:1.f;
 // Runtime attachments have the same reflected handedness. Keep a valid endpoint
 // rather than manufacturing a singular transform if an invalid caller mixes it.
 if((determinant(b)<0?-1.f:1.f)!=handed)return u<.5f?a:b;
 auto qa=rotation(a,handed),qb=rotation(b,handed);float d=qa.x*qb.x+qa.y*qb.y+qa.z*qb.z+qa.w*qb.w;
 if(d<0){qb={-qb.x,-qb.y,-qb.z,-qb.w};d=-d;}d=std::max(-1.f,std::min(1.f,d));
 float x=1-u,y=u;if(d<.9995f){float angle=std::acos(d),den=std::sin(angle);x=std::sin((1-u)*angle)/den;y=std::sin(u*angle)/den;}
 arsenal::Q q=arsenal::norm({qa.x*x+qb.x*y,qa.y*x+qb.y*y,qa.z*x+qb.z*y,qa.w*x+qb.w*y});
 auto m=arsenal::trs({a.v[3]+(b.v[3]-a.v[3])*u,a.v[7]+(b.v[7]-a.v[7])*u,a.v[11]+(b.v[11]-a.v[11])*u},q,{1,1,handed});return m;
}
}
'''
(O/'sword_carry.h').write_text(s)
