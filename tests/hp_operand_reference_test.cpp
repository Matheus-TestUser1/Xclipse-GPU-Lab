#include "../app/src/main/cpp/hp_operand_reference.h"
#include <cmath>
#include <iostream>
using namespace hp_operands;
static void check(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
static long double hv(uint16_t h){return std::ldexp((long double)units24(h),-24);}
static uint16_t hround(long double x,bool rtz){_Float16 f=(_Float16)x;uint16_t h;std::memcpy(&h,&f,2);if(rtz&&std::fabs(hv(h))>std::fabs(x))h--;return h;}
static long double fround(long double x){volatile float f=(float)x;return f;}
static uint32_t bits(long double x){float f=(float)x;uint32_t b;std::memcpy(&b,&f,4);return b;}
static uint32_t hbits(long double x,bool rtz){return bits(hv(hround(x,rtz)));}
int main(){
 uint32_t data[640]{};hp_operands::initialize(data);unsigned count=0,fullPrecision=0,fusedSplit=0;
 check(data[128]==hp_research::halfAsFloat(UNIFORM_B),"explicit cancellation A");
 for(unsigned phase=0;phase<2;phase++){
  if(phase)matchedSources(data);
  for(unsigned kind=0;kind<9;kind++)for(unsigned lane=0;lane<64;lane++){
   auto in=input(data,lane,kind);auto m=models(in);
   long double a=std::ldexp((long double)in.a,-24),b=hv(in.b),c=hv(in.c);
   for(unsigned r=0;r<2;r++){
    long double ea=hv(hround(a,r)),p=hv(hround(ea*b,r));
    check(equivalent(m[r],hbits(ea*b+c,r)),"early half fused");
    check(equivalent(m[2+r],hbits(p+c,r)),"early half split");
    check(equivalent(m[4+r],bits(ea*b+c)),"early full FP32");
    check(equivalent(m[6+r],bits(p+c)),"half product FP32 sum");
    check(equivalent(m[8+r],hbits(fround(a*b+c),r)),"late half fused32");
    check(equivalent(m[10+r],hbits(fround(fround(a*b)+c),r)),"late half split32");
    check(equivalent(m[12+r],hbits(a*b+c,r)),"late half ideal");
   }
   check(equivalent(m[14],bits(a*b+c)),"late full FP32 fused");
   check(equivalent(m[15],bits(fround(a*b)+c)),"late full FP32 split");
   fullPrecision+=m[0]!=m[4];fusedSplit+=m[0]!=m[2];count++;
   if(lane==0&&kind==0){check(m[0]==0x39800000,"known fused cancellation 2^-12");check(m[2]==0,"known split cancellation zero");}
  }
 }
 check(fullPrecision>100&&fusedSplit>10,"corpus separates half/full FP32 and fused/split");
 check(equivalent(0,0x80000000u),"zero sign convention");
 std::cout<<"PASS HPO "<<count<<" triples x16 independent models; full32 distinctions="<<fullPrecision<<" fused/split="<<fusedSplit<<"\n";
}
