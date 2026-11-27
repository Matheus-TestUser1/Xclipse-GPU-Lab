#pragma once
#include <cstdint>
#include <ostream>
#include <iomanip>
namespace hp_extra {
constexpr int SPECIAL_FIRST=129, BENCH_FIRST=165, END=177;
constexpr unsigned LANES=64, ITERATIONS=256, ACCUMULATORS=4, MAX_GROUPS=32;
constexpr unsigned OUTPUT_BASE=640, BUFFER_WORDS=OUTPUT_BASE+MAX_GROUPS*LANES*ACCUMULATORS;
constexpr uint32_t fp32_cases[]={
 0x00000000u,0x80000000u,0x00000001u,0x007fffffu,
 0x00800000u,0x80000001u,0x33000000u,0x33800000u,
 0x387fc000u,0x38800000u,0x477fe000u,0x47800000u,
 0x7f800000u,0xff800000u,0x7fc12345u,0x7f812345u
};
constexpr uint16_t halves[][3]={
 {0x3c00,0x3c00,0x0000}, {0xbc00,0x3c00,0x8000},
 {0x0000,0x7c00,0x0001}, {0x0400,0x0001,0xfc00}
};
constexpr uint32_t sentinel(unsigned lane){return ((0xb400u+(lane&63u))<<16)|(0x3400u+(lane&63u));}
constexpr unsigned position(int id){return unsigned((id-SPECIAL_FIRST)%12/4);}
constexpr unsigned operation(int id){return unsigned((id-SPECIAL_FIRST)%4);}
constexpr unsigned groups(int id){return id>=BENCH_FIRST+6?32u:1u;}
constexpr unsigned benchKind(int id){return unsigned((id-BENCH_FIRST)%6/2);}
constexpr bool dependent(int id){return (id-BENCH_FIRST)%2==0;}
constexpr bool modeSupported(unsigned mode,bool preserve16,bool preserve32,bool flush16,bool flush32,bool signed16,bool signed32){
 return mode==0||(mode<=2&&signed16&&signed32&&(mode==1?(preserve16&&preserve32):(flush16&&flush32)));
}
inline void initializeSpecial(uint32_t* d){
 for(unsigned i=0;i<LANES;i++){
  unsigned context=i/16;
  for(unsigned p=0;p<3;p++){d[128+64*p+i]=fp32_cases[i%16];d[320+64*p+i]=halves[context][p];}
  d[576+i]=sentinel(i);
 }
}
inline void initializeBench(uint32_t* d){
 for(unsigned i=0;i<LANES;i++){
  // Finite, nonconstant per-lane operands. Packed components have distinct values.
  d[128+i]=0x3e000000u+(i<<15);
  d[192+i]=0x3bfe3bffu;
  d[256+i]=0x08000400u;
  for(unsigned k=0;k<4;k++)d[320+64*k+i]=((0x3500u+k*32u+i)<<16)|(0x3000u+k*32u+i);
 }
}
inline bool sentinelOK(int id,unsigned lane,uint32_t result){
 unsigned op=operation(id);uint32_t mask=op==2?0xffff0000u:op==3?0xffffu:0u;
 return (result&mask)==(sentinel(lane)&mask);
}
inline void specialResults(std::ostream& o,const uint32_t* d,int id,unsigned expectedWave){
 unsigned badSentinel=0,badWave=0;
 o<<"HPE_ROWS|lane|a32|b32|c32|a16|b16|c16|sentinel|output|wave (bits in hex except lane/wave; selected source determined by test label)\n";
 for(unsigned i=0;i<LANES;i++){
  o<<"HPE_ROW|"<<i<<std::hex;
  for(unsigned p=0;p<3;p++)o<<"|"<<d[128+p*64+i];
  for(unsigned p=0;p<3;p++)o<<"|"<<d[320+p*64+i];
  o<<"|"<<d[576+i]<<"|"<<d[i]<<std::dec<<"|"<<d[64+i]<<"\n";
  badSentinel+=!sentinelOK(id,i,d[i]);badWave+=d[64+i]!=expectedWave;
 }
 o<<"HPE_SUMMARY|sentinel_checked="<<(operation(id)>=2?64:0)<<"|sentinel_mismatches="<<badSentinel<<"|wave_mismatches="<<badWave<<"\n";
 o<<"HPE_LIMIT|raw shader-level output; NaN payload and signed zero are not assumed equivalent; unpack/pack and conversion can affect bits. Paired value preservation does not prove a physical VGPR half-write.\n";
}
inline uint64_t checksum(const uint32_t* d,unsigned words){
 uint64_t h=14695981039346656037ull;
 for(unsigned i=0;i<words;i++)for(unsigned b=0;b<4;b++){h^=(d[i]>>(b*8))&255u;h*=1099511628211ull;}
 return h;
}
}
