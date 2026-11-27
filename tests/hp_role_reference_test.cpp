#include "../app/src/main/cpp/hp_role_reference.h"
#include <cmath>
#include <iostream>
using namespace hp_roles;
static void check(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
static long double hv(uint16_t h){return std::ldexp((long double)units24(h),-24);}
static uint16_t half(long double x,bool rtz){_Float16 f=(_Float16)x;uint16_t h;std::memcpy(&h,&f,2);if(rtz&&std::fabs(hv(h))>std::fabs(x))h--;return h;}
static long double f32(long double x){volatile float f=(float)x;return f;}
static uint32_t bits(long double x,bool rtz=false){float f=(float)x;if(rtz&&std::fabs((long double)f)>std::fabs(x))f=std::nextafter(f,0.0f);uint32_t b;std::memcpy(&b,&f,4);return b;}
static uint32_t hb(long double x,bool rtz){return bits(hv(half(x,rtz)));}
static void verify(Triple t){
 auto m=hp_roles::models(t);long double a=std::ldexp((long double)t.a,-24),b=std::ldexp((long double)t.b,-24),c=std::ldexp((long double)t.c,-24);
 for(unsigned r=0;r<2;r++){
  long double ea=hv(half(a,r)),eb=hv(half(b,r)),ec=hv(half(c,r)),p=hv(half(ea*eb,r));
  check(equivalent(m[r],hb(ea*eb+ec,r)),"role early half fused");
  check(equivalent(m[2+r],hb(p+ec,r)),"role early half split");
  check(equivalent(m[4+r],bits(ea*eb+ec)),"role early FP32 RTE");
  check(equivalent(m[6+r],bits(p+ec)),"role half product FP32 add");
  check(equivalent(m[8+r],hb(f32(a*b+c),r)),"role late half fused32");
  check(equivalent(m[10+r],hb(f32(f32(a*b)+c),r)),"role late half split32");
  check(equivalent(m[12+r],hb(a*b+c,r)),"role late ideal");
  check(equivalent(m[16+r],bits(ea*eb+ec,true)),"role early FP32 RTZ");
  for(unsigned pr=0;pr<2;pr++)for(unsigned ar=0;ar<2;ar++){
   unsigned combo=r*4+pr*2+ar;if(combo==0||combo==7)continue;
   check(equivalent(m[17+combo],hb(hv(half(ea*eb,pr))+ec,ar)),"role mixed rounding");
  }
 }
 check(equivalent(m[14],bits(a*b+c)),"role late FP32 fused");
 check(equivalent(m[15],bits(f32(a*b)+c)),"role late FP32 split");
}
int main(){
 uint32_t data[640]{};hp_roles::initialize(data);unsigned count=0,distinctions[3]{};
 for(unsigned p=0;p<3;p++){
  auto zero=hp_roles::models(hp_roles::input(data,0,p));check(zero[0]==0x39800000&&zero[2]==0,"role cancellation control");
  for(unsigned i=0;i<64;i++){
   auto t=hp_roles::input(data,i,p);verify(t);auto m=hp_roles::models(t);count++;
   distinctions[p]+=m[0]!=m[1];
  }
  uint32_t before[640];std::copy(data,data+640,before);perturb(data,p);
  for(unsigned i=0;i<640;i++)check(data[i]==before[i]||(i==128+64*p),"perturb only selected source lane zero");
  for(unsigned i=0;i<64;i++){verify(hp_roles::input(data,i,p));count++;}
  std::copy(before,before+640,data);
  check(distinctions[p]>5,"each FP32 position discriminates rounding");
 }
 std::cout<<"PASS HPR "<<count<<" triples x24 independent models, positions A/B/C and perturbation\n";
}
