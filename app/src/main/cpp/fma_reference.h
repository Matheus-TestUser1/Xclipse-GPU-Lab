#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>

// Integer oracle for the bounded, finite binary16 corpus below. No host FP rounding
// mode, FP contraction or Android half intrinsics participate in the reference.
namespace fma_research {
struct Input { uint16_t a,b,c; };
inline int64_t units24(uint16_t h) {
 unsigned e=(h>>10)&31, m=h&1023;
 if(e==31)throw std::runtime_error("oracle requires finite half");
 int64_t v=e?int64_t(m+1024)<<(e-1):m;
 return (h&0x8000)?-v:v;
}
// Input is an exact signed integer in units of 2^-48; corpus stays inside int64.
inline uint16_t round48(int64_t x,bool rtz) {
 uint16_t sign=x<0?0x8000:0; uint64_t v=x<0?uint64_t(-x):uint64_t(x);
 if(!v)return 0;
 int top=0;for(uint64_t t=v;t>>=1;)top++;
 int exponent=top-48+15;
 int shift=exponent<=0?24:top-10;
 uint64_t m=v>>shift,rem=v&((uint64_t(1)<<shift)-1),half=uint64_t(1)<<(shift-1);
 if(!rtz&&(rem>half||(rem==half&&(m&1))))m++;
 if(exponent<=0)return sign|uint16_t(m); // rounding can promote to smallest normal
 if(m==2048){m=1024;exponent++;}
 if(exponent>=31)return sign|(rtz?0x7bff:0x7c00);
 return sign|uint16_t(exponent<<10)|uint16_t(m-1024);
}
inline uint16_t fused(Input t,bool rtz) {
 return round48(units24(t.a)*units24(t.b)+units24(t.c)*int64_t(1<<24),rtz);
}
inline uint16_t split(Input t,bool rtz) {
 uint16_t p=round48(units24(t.a)*units24(t.b),rtz);
 return round48((units24(p)+units24(t.c))*int64_t(1<<24),rtz);
}
inline bool normalOrZero(uint16_t h){return (h&0x7fff)==0||((h&0x7c00)!=0&&(h&0x7c00)!=0x7c00);}
inline uint16_t canonical(uint16_t h){return (h&0x7fff)==0?0:h;}
inline bool equal(uint16_t a,uint16_t b){return canonical(a)==canonical(b);}
inline std::array<Input,128> corpus() {
 std::array<Input,128> out{};
 // Cancellation discriminates fused vs split; ties discriminate RTE vs RTZ.
 out[0]={0x4c01,0x4c01,0xdc02};
 out[1]={0x3c00,0x3c00,0x1600};
 out[2]={0xbc00,0x3c00,0x9600};
 out[3]={0x3e00,0x4000,0x3800};
 uint32_t seed=0x940f16;auto rng=[&](){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;};
 for(unsigned i=4;i<out.size();) {
  Input t{uint16_t(0x3000+rng()%0x1800),uint16_t(0x3000+rng()%0x1800),uint16_t(0x2800+rng()%0x2000)};
  if(rng()&1)t.a|=0x8000;
  if(rng()&1)t.b|=0x8000;
  if(rng()&1)t.c|=0x8000;
  bool valid=true;for(bool rtz:{false,true})if(!normalOrZero(fused(t,rtz))||!normalOrZero(split(t,rtz))||!normalOrZero(round48(units24(t.a)*units24(t.b),rtz)))valid=false;
  if(valid)out[i++]=t;
 }
 return out;
}
inline uint32_t pack(uint16_t lo,uint16_t hi){return uint32_t(lo)|(uint32_t(hi)<<16);}
inline void initialize(uint32_t* data) {
 auto c=corpus();for(unsigned i=0;i<64;i++){
  data[128+i]=pack(c[2*i].a,c[2*i+1].a);
  data[192+i]=pack(c[2*i].b,c[2*i+1].b);
  data[256+i]=pack(c[2*i].c,c[2*i+1].c);
 }
}
inline Input input(const uint32_t* data,unsigned element){
 unsigned lane=element/2,shift=(element%2)*16;
 return {uint16_t(data[128+lane]>>shift),uint16_t(data[192+lane]>>shift),uint16_t(data[256+lane]>>shift)};
}
}
