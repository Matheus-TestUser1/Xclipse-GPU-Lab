#pragma once
#include "fma_reference.h"
#include <ostream>
#include <iomanip>
namespace fma_research {
inline void results(std::ostream& o,const uint32_t* data,int shader,unsigned wave,const char* phase) {
 unsigned counts[4]{},waveErrors=0;int mode=(shader-15)/3,kind=(shader-15)%3;
 for(unsigned i=0;i<128;i++) {
  Input t=input(data,i);uint16_t got=uint16_t(data[i/2]>>((i%2)*16));
  uint16_t expected[]={fused(t,false),fused(t,true),split(t,false),split(t,true)};
  o<<"FMA_ROW|"<<phase<<"|"<<i<<std::hex<<"|"<<t.a<<"|"<<t.b<<"|"<<t.c<<"|"<<got;
  for(unsigned m=0;m<4;m++){if(equal(got,expected[m]))counts[m]++;o<<"|"<<expected[m];}
  o<<std::dec<<"\n";
 }
 for(unsigned i=0;i<64;i++){unsigned actual=data[64+i];if((actual!=32&&actual!=64)||(wave&&actual!=wave))waveErrors++;}
 o<<"FMA_MODELS|phase="<<phase<<"|fused_RTE="<<counts[0]<<"/128|fused_RTZ="<<counts[1]<<"/128|split_RTE="<<counts[2]<<"/128|split_RTZ="<<counts[3]<<"/128\n";
 o<<"FMA_WAVE|requested="<<wave<<"|shader_reported="<<data[64]<<"|mismatches="<<waveErrors<<"/64\n";
 if(kind==2&&mode)o<<(counts[mode==1?2:3]==128?"PASS":"FAIL")<<" FMA split control phase="<<phase<<" explicit_rounding="<<(mode==1?"RTE":"RTZ")<<"\n";
 else o<<"OBSERVATION FMA phase="<<phase<<"; matching a model is shader-level evidence, not an isolated instruction proof or a conformance verdict\n";
 if(waveErrors)o<<"FAIL FMA subgroup result\n";
}
}
