#pragma once
#include "hp_reference.h"
#include <algorithm>

namespace hp_operands {
using fma_research::units24;
using fma_research::round48;
using hp_research::round32Rte;
constexpr int64_t SCALE=1LL<<24;
constexpr uint16_t UNIFORM_B=0x4c01,UNIFORM_C=0xdc02;
constexpr unsigned MODEL_COUNT=16;
inline const char* modelName(unsigned m){
 static const char* names[]={"early_half_fused_RTE","early_half_fused_RTZ","early_half_split_RTE","early_half_split_RTZ",
 "early_f32_fused_RTEinput","early_f32_fused_RTZinput","early_half_product_f32_add_RTE","early_half_product_f32_add_RTZ",
 "late_half_fused32_RTE","late_half_fused32_RTZ","late_half_split32_RTE","late_half_split32_RTZ",
 "late_half_ideal_RTE","late_half_ideal_RTZ","late_f32_fused","late_f32_split"};return names[m];
}
// Exact integer units of 2^-48 -> normal FP32 or zero, with RTE32.
inline uint32_t floatBits(int64_t x){
 x=round32Rte(x);if(!x)return 0;
 uint64_t v=x<0?uint64_t(-x):uint64_t(x);int top=0;for(uint64_t t=v;t>>=1;)top++;
 uint64_t mantissa=top>23?v>>(top-23):v<<(23-top);
 return (x<0?0x80000000u:0u)|(uint32_t(top-48+127)<<23)|uint32_t(mantissa&0x7fffff);
}
inline bool equivalent(uint32_t a,uint32_t b){return a==b||((a|b)&0x7fffffff)==0;}
struct Input {int64_t a;uint16_t b,c;};
inline Input input(const uint32_t* data,unsigned lane,unsigned kind){
 unsigned shift=kind==8?16:0;
 uint16_t b=uint16_t(data[256+lane]>>shift),c=uint16_t(data[320+lane]>>shift);
 if(kind==1||kind==5){b=uint16_t(data[384]);c=uint16_t(data[385]);}
 int64_t a=hp_research::floatUnits24(data[128+lane]);
 if(kind==2)a=-a;
 if(kind==3){if(a<0)a=-a;c^=0x8000;}
 return {a,b,c};
}
inline std::array<uint32_t,MODEL_COUNT> models(Input in){
 std::array<uint32_t,MODEL_COUNT> out{};
 int64_t b=units24(in.b),c=units24(in.c)*SCALE,product=in.a*b,sum=product+c;
 for(unsigned r=0;r<2;r++){
  uint16_t a=round48(in.a*SCALE,r),p=round48(units24(a)*b,r);
  int64_t ep=units24(a)*b;
  out[r]=hp_research::halfAsFloat(round48(ep+c,r));
  out[2+r]=hp_research::halfAsFloat(round48(units24(p)*SCALE+c,r));
  out[4+r]=floatBits(ep+c);
  out[6+r]=floatBits(units24(p)*SCALE+c);
  out[8+r]=hp_research::halfAsFloat(round48(round32Rte(sum),r));
  out[10+r]=hp_research::halfAsFloat(round48(round32Rte(round32Rte(product)+c),r));
  out[12+r]=hp_research::halfAsFloat(round48(sum,r));
 }
 out[14]=floatBits(sum);out[15]=floatBits(round32Rte(product)+c);return out;
}
inline void matchedSources(uint32_t* data){
 for(unsigned i=0;i<64;i++){data[256+i]=fma_research::pack(UNIFORM_B,UNIFORM_B);data[320+i]=fma_research::pack(UNIFORM_C,UNIFORM_C);}
}
inline void initialize(uint32_t* data){
 data[384]=UNIFORM_B;data[385]=UNIFORM_C;
 static const uint16_t bs[]={0x3400,0x3800,0x3c00,0x3c01,0x3e00,0x4000,0x4200,0xbc01,0xc400,0x4c01};
 static const uint16_t cs[]={0x3800,0xb800,0x3c01,0xbc01,0x4400,0xc400,0x5000,0xd000,0x5800,0xdc02};
 uint32_t seed=0x940011;
 auto rng=[&](){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;};
 for(unsigned i=0,attempts=0;i<64;){
  if(++attempts>100000)throw std::runtime_error("HP operand corpus construction failed");
  uint32_t a=i==0?hp_research::halfAsFloat(UNIFORM_B):0x3f000000+rng()%0x03000000;
  if(i&&rng()%2)a|=0x80000000;
  data[128+i]=a;
  data[256+i]=i==0?fma_research::pack(UNIFORM_B,0x3c01):fma_research::pack(bs[rng()%10],bs[rng()%10]);
  data[320+i]=i==0?fma_research::pack(UNIFORM_C,0xbc01):fma_research::pack(cs[rng()%10],cs[rng()%10]);
  // Reject subnormal half intermediates/results across every modifier/source path,
  // including the matched-source phase. Exact integer products stay < 2^61.
  bool valid=true;
  try{for(unsigned kind=0;kind<9;kind++){
   auto in=input(data,i,kind);models(in);
   for(bool r:{false,true})if(!fma_research::normalOrZero(round48(units24(round48(in.a*SCALE,r))*units24(in.b),r)))valid=false;
   in.b=UNIFORM_B;in.c=kind==3?uint16_t(UNIFORM_C^0x8000):UNIFORM_C;models(in);
  }}catch(const std::runtime_error&){valid=false;}
  if(valid)i++;
 }
}
inline void results(std::ostream& o,const uint32_t* data,int shader,unsigned wave,const char* phase){
 unsigned kind=(shader-42)%9,mode=(shader-42)/9,counts[MODEL_COUNT]{},badWave=0;
 for(unsigned i=0;i<64;i++){
  auto in=input(data,i,kind);auto expected=models(in);
  o<<"HPO_ROW|"<<phase<<"|"<<i<<std::hex<<"|"<<data[128+i]<<"|"<<in.b<<"|"<<in.c<<"|"<<data[i];
  for(unsigned m=0;m<MODEL_COUNT;m++){if(equivalent(data[i],expected[m]))counts[m]++;o<<"|"<<expected[m];}
  o<<std::dec<<"\n";
  if((data[64+i]!=32&&data[64+i]!=64)||(wave&&data[64+i]!=wave))badWave++;
 }
 o<<"HPO_MODELS|phase="<<phase;for(unsigned m=0;m<MODEL_COUNT;m++)o<<"|"<<modelName(m)<<"="<<counts[m]<<"/64";o<<"\n";
 o<<"HPO_WAVE|requested="<<wave<<"|shader_reported="<<data[64]<<"|mismatches="<<badWave<<"/64\n";
 if(kind==7&&mode)o<<(counts[2+(mode==2)]==64?"PASS":"FAIL")<<" HPO strict split phase="<<phase<<"\n";
 else o<<"OBSERVATION HPO phase="<<phase<<"; complete-shader models, RTE32 assumption, zero sign ignored.\n";
 if(badWave)o<<"FAIL HPO subgroup\n";
}
}
