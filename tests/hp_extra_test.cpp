#include "../app/src/main/cpp/hp_extra.h"
#include <cassert>
#include <sstream>
#include <vector>
int main(){
 using namespace hp_extra;
 std::vector<uint32_t> d(BUFFER_WORDS);
 initializeSpecial(d.data());
 for(unsigned i=0;i<64;i++){assert(d[128+i]==fp32_cases[i%16]);assert(d[192+i]==d[128+i]);assert(d[576+i]==sentinel(i));assert(sentinelOK(131,i,sentinel(i)));assert(!sentinelOK(131,i,sentinel(i)^0x10000));assert(!sentinelOK(132,i,sentinel(i)^1));assert(sentinelOK(129,i,0));}
 for(int id=129;id<165;id++){assert(position(id)<3);assert(operation(id)<4);}
 assert(modeSupported(0,false,false,false,false,false,false));
 assert(modeSupported(1,true,true,false,false,true,true));
 assert(!modeSupported(1,true,false,true,true,true,true));
 assert(modeSupported(2,false,false,true,true,true,true));
 assert(!modeSupported(2,true,true,true,true,true,false));
 assert(!modeSupported(3,true,true,true,true,true,true));
 initializeBench(d.data());
 for(int id=165;id<177;id++){assert(benchKind(id)<3);assert(groups(id)==(id<171?1:32));assert(dependent(id)==(id%2==1));}
 for(int id=177;id<189;id++){assert(groups(id)==(id<183?1:32));assert(benchKind(id)==0);}
 assert(d[576]==256&&d[639]==256);
 assert(BUFFER_WORDS==8832);
 auto h=checksum(d.data(),640);d[128]^=1;assert(h!=checksum(d.data(),640));
 std::ostringstream report;specialResults(report,d.data(),129,64);assert(report.str().find("HPE_ROW|63|")!=std::string::npos);
}
