#pragma once
#include "hp_operand_reference.h"

namespace hp_roles {
using namespace hp_operands;
inline bool modeSupported(unsigned mode,bool rte16,bool rtz16,bool rte32,bool rtz32,bool independent){
 if(mode==0)return true;
 if(mode>4)return false;
 bool halfRte=mode==1||mode==4,fullRtz=mode==3||mode==4;
 return (halfRte?rte16:rtz16)&&(fullRtz?rtz32:rte32)&&((mode!=2&&mode!=4)||independent);
}
inline unsigned splitModel(unsigned mode){return 2+(mode==2||mode==3);}
struct Triple {int64_t a,b,c;};
inline Triple input(const uint32_t* data,unsigned lane,unsigned position){
 return {position==0?hp_research::floatUnits24(data[128+lane]):units24(uint16_t(data[320+lane])),
         position==1?hp_research::floatUnits24(data[192+lane]):units24(uint16_t(data[384+lane])),
         position==2?hp_research::floatUnits24(data[256+lane]):units24(uint16_t(data[448+lane]))};
}
inline std::array<uint32_t,MODEL_COUNT> models(Triple t){return modelsUnits(t.a,t.b,t.c);}
inline void perturb(uint32_t* data,unsigned position){data[128+64*position]+=0x1001;}
inline void initialize(uint32_t* data){
 uint32_t seed=0x940012;auto rng=[&](){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;};
 for(unsigned i=0,attempts=0;i<64;){
  if(++attempts>100000)throw std::runtime_error("HP role corpus construction failed");
  for(unsigned p=0;p<3;p++){
   uint32_t bits=i==0?hp_research::halfAsFloat(p==2?UNIFORM_C:UNIFORM_B):0x3f000000+rng()%(p==2?0x05000000:0x03000000);
   if(i&&rng()%2)bits|=0x80000000;
   data[128+64*p+i]=bits;
   data[320+64*p+i]=round48(hp_research::floatUnits24(bits)*SCALE,false);
  }
  bool valid=true;
  try{for(unsigned p=0;p<3;p++){
   auto t=input(data,i,p);models(t);
   for(bool r:{false,true})if(!fma_research::normalOrZero(round48(units24(round48(t.a*SCALE,r))*units24(round48(t.b*SCALE,r)),r)))valid=false;
   if(i==0){uint32_t old=data[128+64*p];perturb(data,p);models(input(data,0,p));data[128+64*p]=old;}
  }}catch(const std::runtime_error&){valid=false;}
  if(valid)i++;
 }
}
inline void results(std::ostream& o,const uint32_t* data,int shader,unsigned wave,const char* phase){
 unsigned kind=(shader-69)%12,position=kind/4,op=kind%4,mode=(shader-69)/12;
 unsigned count[MODEL_COUNT]{},badWave=0;
 for(unsigned i=0;i<64;i++){
  auto t=input(data,i,position);auto expected=models(t);
  o<<"HPR_ROW|"<<phase<<"|"<<i<<std::hex;
  for(unsigned p=0;p<3;p++)o<<"|"<<data[128+64*p+i];
  for(unsigned p=0;p<3;p++)o<<"|"<<data[320+64*p+i];
  o<<"|"<<data[i];
  for(unsigned m=0;m<MODEL_COUNT;m++){if(equivalent(data[i],expected[m]))count[m]++;o<<"|"<<expected[m];}
  o<<std::dec<<"\n";
  if((data[64+i]!=32&&data[64+i]!=64)||(wave&&data[64+i]!=wave))badWave++;
 }
 o<<"HPR_MODELS|phase="<<phase;for(unsigned m=0;m<MODEL_COUNT;m++)o<<"|"<<modelName(m)<<"="<<count[m]<<"/64";o<<"\n";
 o<<"HPR_WAVE|requested="<<wave<<"|shader_reported="<<data[64]<<"|mismatches="<<badWave<<"/64\n";
 if(op==3&&mode)o<<(count[splitModel(mode)]==64?"PASS":"FAIL")<<" HPR strict split phase="<<phase<<"\n";
 else o<<"OBSERVATION HPR phase="<<phase<<"; shader-level models, FP32 source position is GLSL position and may be reordered in ISA.\n";
 if(badWave)o<<"FAIL HPR subgroup\n";
}
}
