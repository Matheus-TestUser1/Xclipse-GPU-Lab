#include "../app/src/main/cpp/hp_reference.h"
#include <cmath>
#include <iostream>
using namespace hp_research;
static void check(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
static long double halfValue(uint16_t h){return std::ldexp((long double)units24(h),-24);}
static uint16_t half(long double x,bool rtz){_Float16 f=(_Float16)x;uint16_t h;std::memcpy(&h,&f,2);if(rtz&&std::fabs(halfValue(h))>std::fabs(x))h--;return h;}
static long double f32Rte(long double x){volatile float v=(float)x;return v;}
int main(){
 uint32_t data[640]{};hp_research::initialize(data);unsigned earlyLate=0,rounding=0;
 for(unsigned kind=0;kind<2;kind++)for(unsigned lane=0;lane<64;lane++){
  int64_t x=source(data,lane,kind);long double f=std::ldexp((long double)x,-24);auto m=models(x);
  for(unsigned rtz=0;rtz<2;rtz++){
   uint16_t a=half(f,rtz),prod=half(halfValue(a)*1.5L,rtz);
   check(equal(m[EARLY_FUSED_RTE+rtz],half(halfValue(a)*1.5L+0.5L,rtz)),"early fused independent oracle");
   check(equal(m[EARLY_SPLIT_RTE+rtz],half(halfValue(prod)+0.5L,rtz)),"early split independent oracle");
   check(equal(m[LATE_FUSED32_RTE+rtz],half(f32Rte(f*1.5L+0.5L),rtz)),"late fused32 independent oracle");
   check(equal(m[LATE_SPLIT32_RTE+rtz],half(f32Rte(f32Rte(f*1.5L)+0.5L),rtz)),"late split32 independent oracle");
   check(equal(m[LATE_IDEAL_RTE+rtz],half(f*1.5L+0.5L,rtz)),"late ideal independent oracle");
   for(unsigned j=rtz;j<MODEL_COUNT;j+=2){check(normalOrZero(m[j]),"normal results");float expanded=(float)(_Float16)halfValue(m[j]);uint32_t bits;std::memcpy(&bits,&expanded,4);check(halfAsFloat(m[j])==bits,"half-to-FP32 bits");}
  }
  if(m[EARLY_FUSED_RTE]!=m[LATE_FUSED32_RTE])earlyLate++;
  if(m[EARLY_FUSED_RTE]!=m[EARLY_FUSED_RTZ])rounding++;
 }
 check(earlyLate>10&&rounding>10,"corpus must discriminate conversion and rounding");
 data[128]=0x3f801001;data[192]=2049;for(unsigned kind=0;kind<6;kind++)for(auto h:models(source(data,0,kind)))check(normalOrZero(h),"perturbation corpus");
 std::cout<<"PASS HP integer reference, 10 host models, conversion boundaries; early/late distinctions="<<earlyLate<<" rounding="<<rounding<<"\n";
}
