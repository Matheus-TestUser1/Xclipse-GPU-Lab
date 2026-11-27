// Host utility: feed an exported HPO raw TXT on stdin; no Vulkan/device required.
#include "../app/src/main/cpp/hp_operand_reference.h"
#include <iostream>
#include <sstream>
#include <map>
#include <vector>
int main(){
 using namespace hp_operands;
 const std::string kinds[]={"Fma half per-lane lower","Fma half uniform push","Fma half negate A","Fma half abs A negate C","mul-add half per-lane","mul-add half uniform push","Fma promoted half keep FP32","split half NoContraction","Fma half per-lane upper"};
 struct Phase {unsigned rows=0;uint32_t mask=(1u<<MODEL_COUNT)-1;};
 std::map<std::string,Phase> phases;std::string line,label;int kind=-1;unsigned rows=0,legacyDifferences=0;
 try{while(std::getline(std::cin,line)){
  if(line.rfind("[TEST]",0)==0){label=line;kind=-1;for(unsigned k=0;k<9;k++)if(line.find("HPO "+kinds[k]+" ")!=std::string::npos)kind=int(k);}
  if(line.rfind("HPO_ROW|",0)!=0)continue;
  if(kind<0)throw std::runtime_error("HPO row without recognized test");
  std::vector<std::string> p;std::istringstream ss(line);std::string field;while(std::getline(ss,field,'|'))p.push_back(field);
  if(p.size()<23)throw std::runtime_error("incomplete HPO row");
  int64_t a=hp_research::floatUnits24(uint32_t(std::stoul(p[3],nullptr,16)));
  if(kind==2)a=-a;
  if(kind==3&&a<0)a=-a;
  auto m=models({a,uint16_t(std::stoul(p[4],nullptr,16)),uint16_t(std::stoul(p[5],nullptr,16))});
  uint32_t actual=uint32_t(std::stoul(p[6],nullptr,16));auto& phase=phases[label+"|"+p[1]];phase.rows++;rows++;
  for(unsigned i=0;i<MODEL_COUNT;i++)if(!equivalent(m[i],actual))phase.mask&=~(1u<<i);
  for(unsigned i=0;i<16;i++)if(!equivalent(m[i],uint32_t(std::stoul(p[7+i],nullptr,16))))legacyDifferences++;
 }}catch(const std::exception& e){std::cerr<<"Replay error: "<<e.what()<<"\n";return 1;}
 unsigned explained=0,incomplete=0;
 for(const auto& p:phases){if(p.second.rows!=64)incomplete++;else if(p.second.mask)explained++;else std::cout<<"UNMODELED "<<p.first<<"\n";}
 std::cout<<"HPO_REPLAY rows="<<rows<<" phases="<<phases.size()<<" fully_explained="<<explained<<" legacy_model_differences="<<legacyDifferences<<" incomplete="<<incomplete<<"\n";
 return rows&&legacyDifferences==0&&incomplete==0&&explained==phases.size()?0:1;
}
