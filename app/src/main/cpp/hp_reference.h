#pragma once
#include "fma_reference.h"
#include <cstring>
#include <ostream>
#include <iomanip>

namespace hp_research {
using namespace fma_research;
// Bounded FP32 corpus: zero or |x| in [0.5,8192), and integer sources <=8192.
// Every such x is exact in units of 2^-24. Products by 1.5 fit signed int64.
inline int64_t floatUnits24(uint32_t bits){
 if((bits&0x7fffffff)==0)return 0;
 unsigned e=(bits>>23)&255;
 if(e<126||e>139)throw std::runtime_error("FP32 outside bounded HP corpus");
 int64_t v=int64_t((bits&0x7fffff)|0x800000)<<(e-126);
 return bits&0x80000000?-v:v;
}
inline uint32_t halfAsFloat(uint16_t h){
 if(!normalOrZero(h))throw std::runtime_error("HP corpus requires normal half result");
 return (uint32_t(h&0x8000)<<16)|((h&0x7fff)?(uint32_t(((h>>10)&31)+112)<<23)|(uint32_t(h&1023)<<13):0);
}
inline int64_t round32Rte(int64_t x){
 bool negative=x<0;uint64_t v=negative?uint64_t(-x):uint64_t(x);if(!v)return 0;
 int top=0;for(uint64_t t=v;t>>=1;)top++;
 int shift=top-23;if(shift<=0)return x;
 uint64_t m=v>>shift,rem=v&((uint64_t(1)<<shift)-1),mid=uint64_t(1)<<(shift-1);
 if(rem>mid||(rem==mid&&(m&1)))m++;
 int64_t result=int64_t(m<<shift);return negative?-result:result;
}
enum Model { EARLY_FUSED_RTE,EARLY_FUSED_RTZ,EARLY_SPLIT_RTE,EARLY_SPLIT_RTZ,
 LATE_FUSED32_RTE,LATE_FUSED32_RTZ,LATE_SPLIT32_RTE,LATE_SPLIT32_RTZ,LATE_IDEAL_RTE,LATE_IDEAL_RTZ,MODEL_COUNT };
inline const char* modelName(unsigned i){
 static const char* names[]={"early_fused_RTE","early_fused_RTZ","early_split_RTE","early_split_RTZ","late_fused32_RTE","late_fused32_RTZ","late_split32_RTE","late_split32_RTZ","late_ideal_RTE","late_ideal_RTZ"};return names[i];
}
inline std::array<uint16_t,MODEL_COUNT> models(int64_t x){
 std::array<uint16_t,MODEL_COUNT> out{};
 int64_t product=x*units24(0x3e00),sum=product+units24(0x3800)*int64_t(1<<24);
 for(unsigned rtz=0;rtz<2;rtz++){
  Input early{round48(x*int64_t(1<<24),rtz),0x3e00,0x3800};
  out[EARLY_FUSED_RTE+rtz]=fused(early,rtz);
  out[EARLY_SPLIT_RTE+rtz]=split(early,rtz);
  out[LATE_FUSED32_RTE+rtz]=round48(round32Rte(sum),rtz);
  out[LATE_SPLIT32_RTE+rtz]=round48(round32Rte(round32Rte(product)+units24(0x3800)*int64_t(1<<24)),rtz);
  out[LATE_IDEAL_RTE+rtz]=round48(sum,rtz);
 }
 return out;
}
inline void initialize(uint32_t* data){
 static const uint32_t floating[]={
  0x3f000000,0x3f000001,0x3f7fffff,0x3f800000,0x3f800fff,0x3f801000,0x3f801001,0x3f801fff,
  0x3f802000,0x3f803000,0x3ffff000,0x3ffff001,0x40001000,0x41801000,0x447ff000,0x44fff000,
  0x45001000,0x45001001,0x457ff000,0x457fffff,0xbf000000,0xbf000001,0xbf7fffff,0xbf800000,
  0xbf800fff,0xbf801000,0xbf801001,0xbf801fff,0xbf802000,0xbf803000,0xbffff000,0xbffff001,
  0xc0001000,0xc1801000,0xc47ff000,0xc4fff000,0xc5001000,0xc5001001,0xc57ff000,0xc57fffff};
 static const uint32_t integers[]={1,2,3,4,5,6,7,8,0,15,16,31,32,63,64,127,128,255,256,511,512,1023,1024,2047,2048,2049,2050,2051,4093,4094,4095,4096,4097,4098,8190,8191,8192};
 uint32_t seed=0x940c0a;
 for(unsigned i=0;i<64;i++){
  seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;
  uint32_t x=i<40?floating[i]:(0x3f000000+(seed%0x06000000));
  if(i>=40&&i%2)x|=0x80000000;
  data[128+i]=x;data[192+i]=integers[i%(sizeof(integers)/sizeof(integers[0]))];
  data[256+i]=0x3e00;data[320+i]=0x3800;
 }
}
inline int64_t source(const uint32_t* data,unsigned i,unsigned kind){
 if(kind==0){if(data[192+i]>8192)throw std::runtime_error("HP uint input outside corpus");return int64_t(data[192+i])<<24;}
 return floatUnits24(data[128+i]);
}
inline void results(std::ostream& o,const uint32_t* data,int shader,unsigned wave,const char* phase){
 unsigned count[MODEL_COUNT]{},waveErrors=0;unsigned kind=(shader-24)%6,mode=(shader-24)/6;
 for(unsigned i=0;i<64;i++){
  auto expected=models(source(data,i,kind));
  o<<"HP_ROW|"<<phase<<"|"<<i<<std::hex<<"|"<<data[128+i]<<"|"<<data[192+i]<<"|"<<data[i];
  for(unsigned m=0;m<MODEL_COUNT;m++){if(data[i]==halfAsFloat(expected[m]))count[m]++;o<<"|"<<expected[m];}
  o<<std::dec<<"\n";
  if((data[64+i]!=32&&data[64+i]!=64)||(wave&&data[64+i]!=wave))waveErrors++;
 }
 o<<"HP_MODELS|phase="<<phase;for(unsigned m=0;m<MODEL_COUNT;m++)o<<"|"<<modelName(m)<<"="<<count[m]<<"/64";o<<"\n";
 o<<"HP_WAVE|requested="<<wave<<"|shader_reported="<<data[64]<<"|mismatches="<<waveErrors<<"/64\n";
 if(kind==4&&mode)o<<(count[EARLY_SPLIT_RTE+(mode==2)]==64?"PASS":"FAIL")<<" HP strict split control phase="<<phase<<"\n";
 else o<<"OBSERVATION HP phase="<<phase<<"; models characterize full shaders, not one instruction. FP32 arithmetic models assume RTE32.\n";
 if(waveErrors)o<<"FAIL HP subgroup\n";
}
}
