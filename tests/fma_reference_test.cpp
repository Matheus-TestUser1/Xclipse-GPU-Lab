#include "../app/src/main/cpp/fma_reference.h"
#include <cmath>
#include <cstring>
#include <iostream>
using namespace fma_research;
static long double value(uint16_t h){return std::ldexp((long double)units24(h),-24);}
static uint16_t host(long double exact,bool rtz){
 _Float16 half=(_Float16)exact;uint16_t bits;std::memcpy(&bits,&half,2);
 if(rtz&&std::fabs(value(bits))>std::fabs(exact))bits--;
 return bits;
}
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){
 check(fused({0x4c01,0x4c01,0xdc02},false)==0x0c00,"fused cancellation");
 check(split({0x4c01,0x4c01,0xdc02},false)==0,"split cancellation");
 check(fused({0x3c00,0x3c00,0x1600},false)==0x3c02,"tie even");
 check(fused({0x3c00,0x3c00,0x1600},true)==0x3c01,"tie toward zero");
 auto c=corpus();unsigned distinguishes=0;
 for(auto t:c)for(bool rtz:{false,true}){
  check(equal(fused(t,rtz),host(value(t.a)*value(t.b)+value(t.c),rtz)),"fused independent host oracle");
  uint16_t product=host(value(t.a)*value(t.b),rtz);
  check(equal(split(t,rtz),host(value(product)+value(t.c),rtz)),"split independent host oracle");
  if(!equal(fused(t,rtz),split(t,rtz)))distinguishes++;
 }
 check(distinguishes>5,"corpus must distinguish fusion");
 uint32_t data[640]{};initialize(data);for(unsigned i=0;i<128;i++){auto t=input(data,i);check(t.a==c[i].a&&t.b==c[i].b&&t.c==c[i].c,"packed layout");}
 // Reference also handles finite subnormal boundaries; these are excluded from GPU probes.
 for(unsigned h=0;h<0x6000;h++)check(equal(round48(units24(h)*int64_t(1<<24),false),h),"half round trip");
 std::cout<<"PASS exact half oracle, ties, cancellation, packed corpus (fusion distinctions="<<distinguishes<<")\n";
}
