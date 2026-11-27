#include <vulkan/vulkan.h>
#include "compute_generated.inc"
#include "fma_results.h"
#include "hp_reference.h"
#include "hp_operand_reference.h"
#include "hp_role_reference.h"
#include <vector>
#include <string>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <chrono>
namespace {
void check(VkResult r,const char* step){if(r!=VK_SUCCESS)throw std::runtime_error(std::string(step)+" VkResult="+std::to_string(r));}
struct Instance {VkInstance i{};~Instance(){if(i)vkDestroyInstance(i,nullptr);}};
struct Run {
 VkDevice d{};VkBuffer b{};VkDeviceMemory mem{};VkDescriptorSetLayout sl{};VkPipelineLayout pl{};VkShaderModule sm{};VkPipeline pipe{};VkDescriptorPool pool{};VkCommandPool cp{};VkFence fence{};VkQueryPool timestamps{};void* map{};
 ~Run(){if(!d)return;if(map)vkUnmapMemory(d,mem);if(timestamps)vkDestroyQueryPool(d,timestamps,nullptr);if(fence)vkDestroyFence(d,fence,nullptr);if(cp)vkDestroyCommandPool(d,cp,nullptr);if(pool)vkDestroyDescriptorPool(d,pool,nullptr);if(pipe)vkDestroyPipeline(d,pipe,nullptr);if(sm)vkDestroyShaderModule(d,sm,nullptr);if(pl)vkDestroyPipelineLayout(d,pl,nullptr);if(sl)vkDestroyDescriptorSetLayout(d,sl,nullptr);if(b)vkDestroyBuffer(d,b,nullptr);if(mem)vkFreeMemory(d,mem,nullptr);vkDestroyDevice(d,nullptr);}
};
void test(std::ostream& o,VkPhysicalDevice p,uint32_t family,int id,uint32_t wave,bool useExt,bool amdInfo,bool executableInfo,uint32_t timestampBits,float timestampPeriod,uint32_t defaultWave){
 Run r;float pri=1;VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};qi.queueFamilyIndex=family;qi.queueCount=1;qi.pQueuePriorities=&pri;
 VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};bool fmaTest=id>=15&&id<=23;bool hpTest=id>=24&&id<42;bool hpoTest=id>=42&&id<69;bool hprTest=id>=69;bool mat=id==4||(id>=12&&id<=14);f12.shaderFloat16=id==2||id==4||id==10||id==12||fmaTest||hpTest||hpoTest||hprTest;f12.shaderInt8=id==3||id==6||id==13||id==14;f12.vulkanMemoryModel=mat;
 VkPhysicalDeviceSubgroupSizeControlFeatures sz{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES};sz.subgroupSizeControl=wave!=0;
 VkPhysicalDeviceCooperativeMatrixFeaturesKHR cm{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_FEATURES_KHR};cm.cooperativeMatrix=mat;
 VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR pe{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_EXECUTABLE_PROPERTIES_FEATURES_KHR};pe.pipelineExecutableInfo=executableInfo;
 VkPhysicalDeviceShaderIntegerDotProductFeatures dot{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INTEGER_DOT_PRODUCT_FEATURES};dot.shaderIntegerDotProduct=id==6||id==7;
 VkPhysicalDeviceShaderAtomicFloatFeaturesEXT af{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT};af.shaderBufferFloat32AtomicAdd=id==8;
 VkPhysicalDeviceFeatures baseFeatures{};baseFeatures.shaderInt16=id==7;
 void* chain=nullptr;if(id==6||id==7){dot.pNext=chain;chain=&dot;}if(id==8){af.pNext=chain;chain=&af;}if(wave){sz.pNext=chain;chain=&sz;}if(mat){cm.pNext=chain;chain=&cm;}if(executableInfo){pe.pNext=chain;chain=&pe;}f12.pNext=chain;
 VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};di.pNext=&f12;di.pEnabledFeatures=&baseFeatures;di.queueCreateInfoCount=1;di.pQueueCreateInfos=&qi;
 std::vector<const char*> enabled;if(wave&&useExt)enabled.push_back(VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME);if(mat)enabled.push_back(VK_KHR_COOPERATIVE_MATRIX_EXTENSION_NAME);if((id==6||id==7)&&useExt)enabled.push_back(VK_KHR_SHADER_INTEGER_DOT_PRODUCT_EXTENSION_NAME);if(id==8)enabled.push_back(VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME);if(amdInfo)enabled.push_back(VK_AMD_SHADER_INFO_EXTENSION_NAME);if(executableInfo)enabled.push_back(VK_KHR_PIPELINE_EXECUTABLE_PROPERTIES_EXTENSION_NAME);di.enabledExtensionCount=enabled.size();di.ppEnabledExtensionNames=enabled.data();
 check(vkCreateDevice(p,&di,nullptr,&r.d),"device");VkQueue q;vkGetDeviceQueue(r.d,family,0,&q);
 VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};bi.size=640*sizeof(uint32_t);bi.usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;bi.sharingMode=VK_SHARING_MODE_EXCLUSIVE;check(vkCreateBuffer(r.d,&bi,nullptr,&r.b),"buffer");
 VkMemoryRequirements req;vkGetBufferMemoryRequirements(r.d,r.b,&req);VkPhysicalDeviceMemoryProperties mp;vkGetPhysicalDeviceMemoryProperties(p,&mp);uint32_t mt=UINT32_MAX;
 for(uint32_t i=0;i<mp.memoryTypeCount;i++)if((req.memoryTypeBits&(1u<<i))&&((mp.memoryTypes[i].propertyFlags&(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))==(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))){mt=i;break;}
 if(mt==UINT32_MAX)throw std::runtime_error("no host coherent storage memory");
 VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.allocationSize=req.size;ai.memoryTypeIndex=mt;check(vkAllocateMemory(r.d,&ai,nullptr,&r.mem),"allocate");check(vkBindBufferMemory(r.d,r.b,r.mem,0),"bind");check(vkMapMemory(r.d,r.mem,0,VK_WHOLE_SIZE,0,&r.map),"map");
 auto* data=static_cast<uint32_t*>(r.map);for(unsigned i=0;i<640;i++)data[i]=0xffffffffu;for(unsigned i=0;i<64;i++){data[i]=id==8?0u:0xffffffffu;data[i+64]=1+i%8;}if(id>=12&&id<=14){for(unsigned i=0;i<512;i++)data[i]=0xffffffffu;data[512]=id==12?0x3f800000u:1u;data[513]=id==12?0x40000000u:2u;}
 if(fmaTest)fma_research::initialize(data);
 if(hpTest)hp_research::initialize(data);
 if(hpoTest)hp_operands::initialize(data);
 if(hprTest)hp_roles::initialize(data);
 VkDescriptorSetLayoutBinding lb{};lb.binding=0;lb.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;lb.descriptorCount=1;lb.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;VkDescriptorSetLayoutCreateInfo li{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};li.bindingCount=1;li.pBindings=&lb;check(vkCreateDescriptorSetLayout(r.d,&li,nullptr,&r.sl),"descriptor layout");
 VkPipelineLayoutCreateInfo pli{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pli.setLayoutCount=1;pli.pSetLayouts=&r.sl;VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT,0,8};if(hpoTest){pli.pushConstantRangeCount=1;pli.pPushConstantRanges=&push;}check(vkCreatePipelineLayout(r.d,&pli,nullptr,&r.pl),"pipeline layout");
 const uint32_t* code=probe0;size_t bytes=sizeof(probe0);if(id==1){code=probe1;bytes=sizeof(probe1);}if(id==2){code=probe2;bytes=sizeof(probe2);}if(id==3){code=probe3;bytes=sizeof(probe3);}if(id==4){code=probe4;bytes=sizeof(probe4);}if(id==5){code=probe5;bytes=sizeof(probe5);}if(id==6){code=probe6;bytes=sizeof(probe6);}if(id==7){code=probe7;bytes=sizeof(probe7);}if(id==8){code=probe8;bytes=sizeof(probe8);}if(id==9){code=probe9;bytes=sizeof(probe9);}if(id==10){code=probe10;bytes=sizeof(probe10);}if(id==11){code=probe11;bytes=sizeof(probe11);}if(id==12){code=probe12;bytes=sizeof(probe12);}if(id==13){code=probe13;bytes=sizeof(probe13);}if(id==14){code=probe14;bytes=sizeof(probe14);}if(id==15){code=probe15;bytes=sizeof(probe15);}if(id==16){code=probe16;bytes=sizeof(probe16);}if(id==17){code=probe17;bytes=sizeof(probe17);}if(id==18){code=probe18;bytes=sizeof(probe18);}if(id==19){code=probe19;bytes=sizeof(probe19);}if(id==20){code=probe20;bytes=sizeof(probe20);}if(id==21){code=probe21;bytes=sizeof(probe21);}if(id==22){code=probe22;bytes=sizeof(probe22);}if(id==23){code=probe23;bytes=sizeof(probe23);}if(id==24){code=probe24;bytes=sizeof(probe24);}if(id==25){code=probe25;bytes=sizeof(probe25);}if(id==26){code=probe26;bytes=sizeof(probe26);}if(id==27){code=probe27;bytes=sizeof(probe27);}if(id==28){code=probe28;bytes=sizeof(probe28);}if(id==29){code=probe29;bytes=sizeof(probe29);}if(id==30){code=probe30;bytes=sizeof(probe30);}if(id==31){code=probe31;bytes=sizeof(probe31);}if(id==32){code=probe32;bytes=sizeof(probe32);}if(id==33){code=probe33;bytes=sizeof(probe33);}if(id==34){code=probe34;bytes=sizeof(probe34);}if(id==35){code=probe35;bytes=sizeof(probe35);}if(id==36){code=probe36;bytes=sizeof(probe36);}if(id==37){code=probe37;bytes=sizeof(probe37);}if(id==38){code=probe38;bytes=sizeof(probe38);}if(id==39){code=probe39;bytes=sizeof(probe39);}if(id==40){code=probe40;bytes=sizeof(probe40);}if(id==41){code=probe41;bytes=sizeof(probe41);}if(id==42){code=probe42;bytes=sizeof(probe42);}if(id==43){code=probe43;bytes=sizeof(probe43);}if(id==44){code=probe44;bytes=sizeof(probe44);}if(id==45){code=probe45;bytes=sizeof(probe45);}if(id==46){code=probe46;bytes=sizeof(probe46);}if(id==47){code=probe47;bytes=sizeof(probe47);}if(id==48){code=probe48;bytes=sizeof(probe48);}if(id==49){code=probe49;bytes=sizeof(probe49);}if(id==50){code=probe50;bytes=sizeof(probe50);}if(id==51){code=probe51;bytes=sizeof(probe51);}if(id==52){code=probe52;bytes=sizeof(probe52);}if(id==53){code=probe53;bytes=sizeof(probe53);}if(id==54){code=probe54;bytes=sizeof(probe54);}if(id==55){code=probe55;bytes=sizeof(probe55);}if(id==56){code=probe56;bytes=sizeof(probe56);}if(id==57){code=probe57;bytes=sizeof(probe57);}if(id==58){code=probe58;bytes=sizeof(probe58);}if(id==59){code=probe59;bytes=sizeof(probe59);}if(id==60){code=probe60;bytes=sizeof(probe60);}if(id==61){code=probe61;bytes=sizeof(probe61);}if(id==62){code=probe62;bytes=sizeof(probe62);}if(id==63){code=probe63;bytes=sizeof(probe63);}if(id==64){code=probe64;bytes=sizeof(probe64);}if(id==65){code=probe65;bytes=sizeof(probe65);}if(id==66){code=probe66;bytes=sizeof(probe66);}if(id==67){code=probe67;bytes=sizeof(probe67);}if(id==68){code=probe68;bytes=sizeof(probe68);}if(id==69){code=probe69;bytes=sizeof(probe69);}if(id==70){code=probe70;bytes=sizeof(probe70);}if(id==71){code=probe71;bytes=sizeof(probe71);}if(id==72){code=probe72;bytes=sizeof(probe72);}if(id==73){code=probe73;bytes=sizeof(probe73);}if(id==74){code=probe74;bytes=sizeof(probe74);}if(id==75){code=probe75;bytes=sizeof(probe75);}if(id==76){code=probe76;bytes=sizeof(probe76);}if(id==77){code=probe77;bytes=sizeof(probe77);}if(id==78){code=probe78;bytes=sizeof(probe78);}if(id==79){code=probe79;bytes=sizeof(probe79);}if(id==80){code=probe80;bytes=sizeof(probe80);}if(id==81){code=probe81;bytes=sizeof(probe81);}if(id==82){code=probe82;bytes=sizeof(probe82);}if(id==83){code=probe83;bytes=sizeof(probe83);}if(id==84){code=probe84;bytes=sizeof(probe84);}if(id==85){code=probe85;bytes=sizeof(probe85);}if(id==86){code=probe86;bytes=sizeof(probe86);}if(id==87){code=probe87;bytes=sizeof(probe87);}if(id==88){code=probe88;bytes=sizeof(probe88);}if(id==89){code=probe89;bytes=sizeof(probe89);}if(id==90){code=probe90;bytes=sizeof(probe90);}if(id==91){code=probe91;bytes=sizeof(probe91);}if(id==92){code=probe92;bytes=sizeof(probe92);}if(id==93){code=probe93;bytes=sizeof(probe93);}if(id==94){code=probe94;bytes=sizeof(probe94);}if(id==95){code=probe95;bytes=sizeof(probe95);}if(id==96){code=probe96;bytes=sizeof(probe96);}if(id==97){code=probe97;bytes=sizeof(probe97);}if(id==98){code=probe98;bytes=sizeof(probe98);}if(id==99){code=probe99;bytes=sizeof(probe99);}if(id==100){code=probe100;bytes=sizeof(probe100);}if(id==101){code=probe101;bytes=sizeof(probe101);}if(id==102){code=probe102;bytes=sizeof(probe102);}if(id==103){code=probe103;bytes=sizeof(probe103);}if(id==104){code=probe104;bytes=sizeof(probe104);}if(id==105){code=probe105;bytes=sizeof(probe105);}if(id==106){code=probe106;bytes=sizeof(probe106);}if(id==107){code=probe107;bytes=sizeof(probe107);}if(id==108){code=probe108;bytes=sizeof(probe108);}if(id==109){code=probe109;bytes=sizeof(probe109);}if(id==110){code=probe110;bytes=sizeof(probe110);}if(id==111){code=probe111;bytes=sizeof(probe111);}if(id==112){code=probe112;bytes=sizeof(probe112);}if(id==113){code=probe113;bytes=sizeof(probe113);}if(id==114){code=probe114;bytes=sizeof(probe114);}if(id==115){code=probe115;bytes=sizeof(probe115);}if(id==116){code=probe116;bytes=sizeof(probe116);}if(id==117){code=probe117;bytes=sizeof(probe117);}if(id==118){code=probe118;bytes=sizeof(probe118);}if(id==119){code=probe119;bytes=sizeof(probe119);}if(id==120){code=probe120;bytes=sizeof(probe120);}if(id==121){code=probe121;bytes=sizeof(probe121);}if(id==122){code=probe122;bytes=sizeof(probe122);}if(id==123){code=probe123;bytes=sizeof(probe123);}if(id==124){code=probe124;bytes=sizeof(probe124);}if(id==125){code=probe125;bytes=sizeof(probe125);}if(id==126){code=probe126;bytes=sizeof(probe126);}if(id==127){code=probe127;bytes=sizeof(probe127);}if(id==128){code=probe128;bytes=sizeof(probe128);}
 VkShaderModuleCreateInfo si{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};si.codeSize=bytes;si.pCode=code;if(fmaTest||hpTest||hpoTest||hprTest){uint64_t hash=14695981039346656037ull;for(size_t h=0;h<bytes;h++){hash^=reinterpret_cast<const uint8_t*>(code)[h];hash*=1099511628211ull;}o<<"\nSPIRV_FINGERPRINT|fnv1a64="<<std::hex<<hash<<std::dec<<"|bytes="<<bytes<<"\n";}check(vkCreateShaderModule(r.d,&si,nullptr,&r.sm),"shader module");
 VkPipelineShaderStageRequiredSubgroupSizeCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO};rs.requiredSubgroupSize=wave;
 VkComputePipelineCreateInfo pi{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};pi.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;pi.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;pi.stage.module=r.sm;pi.stage.pName="main";if(wave)pi.stage.pNext=&rs;pi.layout=r.pl;if(executableInfo)pi.flags=VK_PIPELINE_CREATE_CAPTURE_INTERNAL_REPRESENTATIONS_BIT_KHR|VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR;check(vkCreateComputePipelines(r.d,VK_NULL_HANDLE,1,&pi,nullptr,&r.pipe),"compute pipeline");
 if(amdInfo){auto fn=(PFN_vkGetShaderInfoAMD)vkGetDeviceProcAddr(r.d,"vkGetShaderInfoAMD");if(fn){size_t bytes=0;auto res=fn(r.d,r.pipe,VK_SHADER_STAGE_COMPUTE_BIT,VK_SHADER_INFO_TYPE_DISASSEMBLY_AMD,&bytes,nullptr);o<<"\nAMD disassembly query="<<res<<" bytes="<<bytes<<"\n";if(res==VK_SUCCESS&&bytes&&bytes<=262144){std::vector<char> txt(bytes+1);res=fn(r.d,r.pipe,VK_SHADER_STAGE_COMPUTE_BIT,VK_SHADER_INFO_TYPE_DISASSEMBLY_AMD,&bytes,txt.data());if(res==VK_SUCCESS)o<<"[AMD driver disassembly]\n"<<txt.data()<<"\n";}VkShaderStatisticsInfoAMD st{};bytes=sizeof(st);if(fn(r.d,r.pipe,VK_SHADER_STAGE_COMPUTE_BIT,VK_SHADER_INFO_TYPE_STATISTICS_AMD,&bytes,&st)==VK_SUCCESS)o<<"AMD statistics VGPR="<<st.resourceUsage.numUsedVgprs<<" SGPR="<<st.resourceUsage.numUsedSgprs<<" LDS bytes="<<st.resourceUsage.ldsUsageSizeInBytes<<"\n";}}
 if(executableInfo){auto fn=(PFN_vkGetPipelineExecutablePropertiesKHR)vkGetDeviceProcAddr(r.d,"vkGetPipelineExecutablePropertiesKHR");auto ir=(PFN_vkGetPipelineExecutableInternalRepresentationsKHR)vkGetDeviceProcAddr(r.d,"vkGetPipelineExecutableInternalRepresentationsKHR");VkPipelineInfoKHR info{VK_STRUCTURE_TYPE_PIPELINE_INFO_KHR};info.pipeline=r.pipe;uint32_t count=0;if(fn&&ir&&fn(r.d,&info,&count,nullptr)==VK_SUCCESS&&count<=32){std::vector<VkPipelineExecutablePropertiesKHR> props(count);for(auto& x:props)x.sType=VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_PROPERTIES_KHR;if(fn(r.d,&info,&count,props.data())==VK_SUCCESS)for(uint32_t i=0;i<count;i++){o<<"Executable "<<props[i].name<<" subgroup="<<props[i].subgroupSize<<"\n";VkPipelineExecutableInfoKHR ei{VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_INFO_KHR};ei.pipeline=r.pipe;ei.executableIndex=i;uint32_t n=0;if(ir(r.d,&ei,&n,nullptr)!=VK_SUCCESS||n>32)continue;std::vector<VkPipelineExecutableInternalRepresentationKHR> reps(n);for(auto& x:reps)x.sType=VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_INTERNAL_REPRESENTATION_KHR;if(ir(r.d,&ei,&n,reps.data())!=VK_SUCCESS)continue;std::vector<std::vector<char>> data(n);bool valid=true;for(uint32_t j=0;j<n;j++){if(reps[j].dataSize>262144){valid=false;break;}data[j].resize(reps[j].dataSize+1);reps[j].pData=data[j].data();}if(!valid)continue;if(ir(r.d,&ei,&n,reps.data())==VK_SUCCESS)for(uint32_t j=0;j<n;j++){o<<"IR "<<reps[j].name<<" text="<<reps[j].isText<<" bytes="<<reps[j].dataSize<<"\n";if(reps[j].isText)o<<data[j].data()<<"\n";}}}}
 VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1};VkDescriptorPoolCreateInfo dpi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};dpi.maxSets=1;dpi.poolSizeCount=1;dpi.pPoolSizes=&ps;check(vkCreateDescriptorPool(r.d,&dpi,nullptr,&r.pool),"pool");
 VkDescriptorSetAllocateInfo da{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};da.descriptorPool=r.pool;da.descriptorSetCount=1;da.pSetLayouts=&r.sl;VkDescriptorSet set;check(vkAllocateDescriptorSets(r.d,&da,&set),"descriptor");
 VkDescriptorBufferInfo db{r.b,0,VK_WHOLE_SIZE};VkWriteDescriptorSet wr{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};wr.dstSet=set;wr.dstBinding=0;wr.descriptorCount=1;wr.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;wr.pBufferInfo=&db;vkUpdateDescriptorSets(r.d,1,&wr,0,nullptr);
 VkCommandPoolCreateInfo cpi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};cpi.queueFamilyIndex=family;check(vkCreateCommandPool(r.d,&cpi,nullptr,&r.cp),"command pool");VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=r.cp;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;VkCommandBuffer cmd;check(vkAllocateCommandBuffers(r.d,&ca,&cmd),"command buffer");
 if(timestampBits){VkQueryPoolCreateInfo qp{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};qp.queryType=VK_QUERY_TYPE_TIMESTAMP;qp.queryCount=2;if(vkCreateQueryPool(r.d,&qp,nullptr,&r.timestamps)!=VK_SUCCESS)r.timestamps=VK_NULL_HANDLE;}
 VkCommandBufferBeginInfo cb{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};cb.flags=(fmaTest||hpTest||hpoTest||hprTest)?0:VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;check(vkBeginCommandBuffer(cmd,&cb),"begin");
 if(r.timestamps){vkCmdResetQueryPool(cmd,r.timestamps,0,2);vkCmdWriteTimestamp(cmd,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,r.timestamps,0);}
 VkMemoryBarrier before{VK_STRUCTURE_TYPE_MEMORY_BARRIER};before.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT;before.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&before,0,nullptr,0,nullptr);
 vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,r.pipe);vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,r.pl,0,1,&set,0,nullptr);if(hpoTest)vkCmdPushConstants(cmd,r.pl,VK_SHADER_STAGE_COMPUTE_BIT,0,8,data+384);vkCmdDispatch(cmd,1,1,1);if(r.timestamps)vkCmdWriteTimestamp(cmd,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,r.timestamps,1);
 VkMemoryBarrier after{VK_STRUCTURE_TYPE_MEMORY_BARRIER};after.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;after.dstAccessMask=VK_ACCESS_HOST_READ_BIT;vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&after,0,nullptr,0,nullptr);check(vkEndCommandBuffer(cmd),"end");
 VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};check(vkCreateFence(r.d,&fi,nullptr,&r.fence),"fence");VkSubmitInfo sub{VK_STRUCTURE_TYPE_SUBMIT_INFO};sub.commandBufferCount=1;sub.pCommandBuffers=&cmd;auto start=std::chrono::steady_clock::now();check(vkQueueSubmit(q,1,&sub,r.fence),"submit");VkResult wait=vkWaitForFences(r.d,1,&r.fence,VK_TRUE,5000000000ull);if(wait==VK_TIMEOUT)std::_Exit(124);check(wait,"fence wait");double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
 if(hprTest){
  o<<"HPR_ROWS|phase|lane|a_f32|b_f32|c_f32|a_half|b_half|c_half|gpu_f32_bits";
  for(unsigned m=0;m<hp_operands::MODEL_COUNT;m++)o<<"|"<<hp_operands::modelName(m)<<"_f32_bits";
  o<<" (hex; effective FP32 input position from test label; other sources use half fields)\n";
  hp_roles::results(o,data,id,wave,"baseline");
  uint32_t original[64];std::copy(data,data+64,original);
  hp_roles::perturb(data,((id-69)%12)/4);
  for(unsigned i=0;i<128;i++)data[i]=0xffffffffu;
  check(vkResetFences(r.d,1,&r.fence),"reset HPR fence");check(vkQueueSubmit(q,1,&sub,r.fence),"HPR perturb submit");
  VkResult second=vkWaitForFences(r.d,1,&r.fence,VK_TRUE,5000000000ull);if(second==VK_TIMEOUT)std::_Exit(124);check(second,"HPR perturb wait");
  hp_roles::results(o,data,id,wave,"perturb_selected_lane0");
  unsigned changed=0;for(unsigned i=1;i<64;i++)if(data[i]!=original[i])changed++;
  o<<(changed?"FAIL":"PASS")<<" HPR lane independence changed_other_lanes="<<changed<<"/63\n";
  o<<"HPR_LIMIT: GLSL position is not an ISA allocation guarantee; check conversion placement and operand order in this shader's ISA.\n";
  return;
 }
 if(hpoTest){
  o<<"HPO_ROWS|phase|lane|original_a_f32|effective_b_f16|effective_c_f16|gpu_f32_bits";
  for(unsigned m=0;m<hp_operands::MODEL_COUNT;m++)o<<"|"<<hp_operands::modelName(m)<<"_f32_bits";
  o<<" (all hex; A modifiers follow test kind; signed zero equivalent)\n";
  hp_operands::results(o,data,id,wave,"varying_coefficients");
  hp_operands::matchedSources(data);
  for(unsigned i=0;i<128;i++)data[i]=0xffffffffu;
  check(vkResetFences(r.d,1,&r.fence),"reset HPO fence");check(vkQueueSubmit(q,1,&sub,r.fence),"HPO matched submit");
  VkResult second=vkWaitForFences(r.d,1,&r.fence,VK_TRUE,5000000000ull);if(second==VK_TIMEOUT)std::_Exit(124);check(second,"HPO matched wait");
  hp_operands::results(o,data,id,wave,"matched_sources");
  o<<"HPO_LIMIT: compare matching-source shaders by actual ISA. Uniform data is an SGPR candidate, not an allocation guarantee. No isolated opcode or WMMA proof.\n";
  return;
 }
 if(hpTest){
  o<<"HP_ROWS|phase|lane|source_f32_bits|source_uint|gpu_f32_bits";
  for(unsigned m=0;m<hp_research::MODEL_COUNT;m++)o<<"|"<<hp_research::modelName(m)<<"_f16";
  o<<" (hex values; constant operands B=1.5, C=0.5)\n";
  hp_research::results(o,data,id,wave,"baseline");
  uint32_t original[64];std::copy(data,data+64,original);
  if((id-24)%6==0)data[192]=2049;else data[128]=0x3f801001;
  for(unsigned i=0;i<128;i++)data[i]=0xffffffffu;
  check(vkResetFences(r.d,1,&r.fence),"reset HP fence");check(vkQueueSubmit(q,1,&sub,r.fence),"HP perturb submit");
  VkResult second=vkWaitForFences(r.d,1,&r.fence,VK_TRUE,5000000000ull);if(second==VK_TIMEOUT)std::_Exit(124);check(second,"HP perturb wait");
  hp_research::results(o,data,id,wave,"perturb_lane0_source");
  unsigned changed=0;for(unsigned i=1;i<64;i++)if(data[i]!=original[i])changed++;
  o<<(changed?"FAIL":"PASS")<<" HP lane independence changed_other_lanes="<<changed<<"/63\n";
  o<<"HP_LIMIT: compiler-selected instructions; hp emission must be confirmed in this shader's ISA before attributing observations to hp. Not a matrix or performance test.\n";
  return;
 }
 if(fmaTest){
  o<<"FMA_ROWS|phase|element|a_f16|b_f16|c_f16|gpu_f16|fused_RTE|fused_RTZ|split_RTE|split_RTZ (hex)\n";
  fma_research::results(o,data,id,wave,"baseline");
  uint32_t original[64];std::copy(data,data+64,original);
  // Reuse the exact pipeline/SPIR-V; change only element zero's A operand.
  data[128]=(data[128]&0xffff0000u)|0x4801u;
  for(unsigned i=0;i<128;i++)data[i]=0xffffffffu;
  check(vkResetFences(r.d,1,&r.fence),"reset perturbation fence");
  check(vkQueueSubmit(q,1,&sub,r.fence),"perturbation submit");
  VkResult second=vkWaitForFences(r.d,1,&r.fence,VK_TRUE,5000000000ull);
  if(second==VK_TIMEOUT)std::_Exit(124);check(second,"perturbation wait");
  fma_research::results(o,data,id,wave,"perturb_lane0_low");
  unsigned changedOthers=0;for(unsigned i=1;i<128;i++){
   uint16_t before=uint16_t(original[i/2]>>((i%2)*16)),after=uint16_t(data[i/2]>>((i%2)*16));
   if(!fma_research::equal(before,after))changedOthers++;
  }
  o<<(changedOthers?"FAIL":"PASS")<<" FMA lane independence changed_other_elements="<<changedOthers<<"/127\n";
  o<<"FMA_LIMIT: the shader may contain several instructions; correlation does not isolate one opcode or establish WMMA. Timing is not a benchmark.\n";
  return;
 }
 unsigned mismatch=0;unsigned checked=64;if(id>=12&&id<=14){uint32_t want=id==12?0x42000000u:32u;uint32_t expectedWave=wave?wave:defaultWave;bool second=expectedWave==32;checked=second?512:256;for(unsigned i=0;i<checked;i++)if(data[i]!=want)mismatch++;if(!second){for(unsigned i=256;i<512;i++)if(data[i]!=0xffffffffu)mismatch++;}if(wave&&checked!=256*(64/wave))mismatch++;}else for(unsigned i=0;i<64;i++){uint32_t expected=(1+i%8)*7+3;if(id==3)expected=(1+i%8)*3+1;if(id==4)expected=0;if(id==5)expected=1+(63-i)%8;if(id==6||id==7)expected=(1+i%8)*5+3;if(id==8)expected=0x40800000u;if(id==10){float v=3.f*(1+i%8)+2.5f;std::memcpy(&expected,&v,4);}if(id==11)expected=((1+i%8)*7+3)^i;if(id==2){float v=(1+i%8)*1.5f+0.5f;std::memcpy(&expected,&v,sizeof(v));}if(id==9){uint32_t cell=data[i]>>16,sum=data[i]&0xffffu,exp=0;if(cell>=16){mismatch++;continue;}for(uint32_t k=0;k<4;k++)exp+=(cell/4+k+1)*(k+cell%4+1);if(sum!=exp)mismatch++;}else if(id==1){uint32_t size=data[i]>>16,sum=data[i]&0xffff;if(size==0||size>128||sum!=std::min(size,64u)||(wave&&size!=wave))mismatch++;}else if(data[i]!=expected)mismatch++;}
 o<<(mismatch?"FAIL":"PASS")<<" mismatch="<<mismatch<<"/"<<checked<<" first=0x"<<std::hex<<data[0]<<std::dec<<" submit_to_fence_ms="<<ms<<"\n";if(r.timestamps){uint64_t t[2]{};auto qr=vkGetQueryPoolResults(r.d,r.timestamps,0,2,sizeof(t),t,sizeof(uint64_t),VK_QUERY_RESULT_64_BIT);if(qr==VK_SUCCESS){uint64_t mask=timestampBits>=64?~0ull:((1ull<<timestampBits)-1);o<<"GPU timestamp delta_ns="<<double((t[1]-t[0])&mask)*timestampPeriod<<"\n";}else o<<"GPU timestamp unavailable: "<<qr<<"\n";}
}
}
static std::string compute_scan_impl(int researchMode){
 bool fmaOnly=researchMode==1,hpOnly=researchMode==2,hpoOnly=researchMode==3,hprOnly=researchMode==4;std::ostringstream o;o<<(hprOnly?"[HP FP32 source roles / RADV]\n":hpoOnly?"[HP operands / modifiers / FP32 research / RADV]\n":hpOnly?"[HP FP32-to-FP16 semantics research / RADV]\n":fmaOnly?"[MGFX2 FMA semantics research / RADV]\n":"[Functional Vulkan compute tests / known answers]\n");
 try{uint32_t api=VK_API_VERSION_1_0;auto ev=(PFN_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(nullptr,"vkEnumerateInstanceVersion");if(ev)check(ev(&api),"instance version");if(api<VK_API_VERSION_1_2)return "INCONCLUSIVE: loader Vulkan <1.2";VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="GPU Lab compute";app.apiVersion=std::min(api,(uint32_t)VK_API_VERSION_1_3);VkInstanceCreateInfo ii{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ii.pApplicationInfo=&app;Instance ins;check(vkCreateInstance(&ii,nullptr,&ins.i),"instance");uint32_t n=0;check(vkEnumeratePhysicalDevices(ins.i,&n,nullptr),"enumerate");std::vector<VkPhysicalDevice> ds(n);check(vkEnumeratePhysicalDevices(ins.i,&n,ds.data()),"devices");
 for(auto p:ds){VkPhysicalDeviceProperties prop{};vkGetPhysicalDeviceProperties(p,&prop);o<<"\n[GPU] "<<prop.deviceName<<"\n";uint32_t ver=std::min(api,prop.apiVersion);if(ver<VK_API_VERSION_1_2||prop.limits.maxComputeWorkGroupInvocations<64||prop.limits.maxComputeWorkGroupSize[0]<64){o<<"SKIP: API/workgroup limit\n";continue;}
 uint32_t k=0;vkGetPhysicalDeviceQueueFamilyProperties(p,&k,nullptr);std::vector<VkQueueFamilyProperties> qs(k);vkGetPhysicalDeviceQueueFamilyProperties(p,&k,qs.data());auto qi=std::find_if(qs.begin(),qs.end(),[](auto x){return x.queueCount&&(x.queueFlags&VK_QUEUE_COMPUTE_BIT);});if(qi==qs.end()){o<<"SKIP no compute queue\n";continue;}uint32_t family=qi-qs.begin();
 uint32_t ec=0;check(vkEnumerateDeviceExtensionProperties(p,nullptr,&ec,nullptr),"extensions");std::vector<VkExtensionProperties> es(ec);check(vkEnumerateDeviceExtensionProperties(p,nullptr,&ec,es.data()),"extensions");bool ext=std::any_of(es.begin(),es.end(),[](auto& e){return std::strcmp(e.extensionName,VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME)==0;});bool sizeAvailable=ver>=VK_API_VERSION_1_3||ext;auto has=[&](const char* name){return std::any_of(es.begin(),es.end(),[&](auto& e){return std::strcmp(e.extensionName,name)==0;});};bool amd=has(VK_AMD_SHADER_INFO_EXTENSION_NAME),exec=has(VK_KHR_PIPELINE_EXECUTABLE_PROPERTIES_EXTENSION_NAME),matrix=has(VK_KHR_COOPERATIVE_MATRIX_EXTENSION_NAME);bool dotAvailable=ver>=VK_API_VERSION_1_3||has(VK_KHR_SHADER_INTEGER_DOT_PRODUCT_EXTENSION_NAME),atomicAvailable=has(VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME);
 VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};VkPhysicalDeviceSubgroupSizeControlFeatures sz{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES};VkPhysicalDeviceFeatures2 ff{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};ff.pNext=&f12;VkPhysicalDeviceCooperativeMatrixFeaturesKHR cm{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_FEATURES_KHR};VkPhysicalDevicePipelineExecutablePropertiesFeaturesKHR pe{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_EXECUTABLE_PROPERTIES_FEATURES_KHR};VkPhysicalDeviceShaderIntegerDotProductFeatures dot{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INTEGER_DOT_PRODUCT_FEATURES};VkPhysicalDeviceShaderAtomicFloatFeaturesEXT af{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT};void* chain=nullptr;if(dotAvailable){dot.pNext=chain;chain=&dot;}if(atomicAvailable){af.pNext=chain;chain=&af;}if(sizeAvailable){sz.pNext=chain;chain=&sz;}if(matrix){cm.pNext=chain;chain=&cm;}if(exec){pe.pNext=chain;chain=&pe;}f12.pNext=chain;vkGetPhysicalDeviceFeatures2(p,&ff);
 VkPhysicalDeviceSubgroupProperties sg{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};VkPhysicalDeviceSubgroupSizeControlProperties sp{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES};VkPhysicalDeviceProperties2 pp{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};pp.pNext=&sg;VkPhysicalDeviceCooperativeMatrixPropertiesKHR cmp{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_PROPERTIES_KHR};void* pc=nullptr;if(sizeAvailable){sp.pNext=pc;pc=&sp;}if(matrix){cmp.pNext=pc;pc=&cmp;}sg.pNext=pc;vkGetPhysicalDeviceProperties2(p,&pp);
 VkPhysicalDeviceFloatControlsProperties fc{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES};
 VkPhysicalDeviceProperties2 fcp{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};fcp.pNext=&fc;vkGetPhysicalDeviceProperties2(p,&fcp);
 bool sub=(sg.supportedStages&VK_SHADER_STAGE_COMPUTE_BIT)&&(sg.supportedOperations&VK_SUBGROUP_FEATURE_BASIC_BIT)&&(sg.supportedOperations&VK_SUBGROUP_FEATURE_ARITHMETIC_BIT);
 auto run=[&](const char* label,int id,bool supported,uint32_t wave=0){o<<"\n[TEST] "<<label<<" | ";if(!supported){o<<"SKIP active driver did not expose required feature (hardware capability undetermined)\n";return;}try{test(o,p,family,id,wave,ver<VK_API_VERSION_1_3,amd,exec&&pe.pipelineExecutableInfo,qi->timestampValidBits,prop.limits.timestampPeriod,sg.subgroupSize);}catch(const std::exception& e){o<<"INCONCLUSIVE "<<e.what()<<"\n";}};
 if(hprOnly){
  o<<"HPR_BUILD|"<<fma_compiler_metadata<<"\n";
  o<<"HPR_FEATURES|Float16="<<f12.shaderFloat16<<"|RTE16="<<fc.shaderRoundingModeRTEFloat16<<"|RTZ16="<<fc.shaderRoundingModeRTZFloat16<<"|RTE32="<<fc.shaderRoundingModeRTEFloat32<<"|RTZ32="<<fc.shaderRoundingModeRTZFloat32<<"|rounding_independence="<<fc.roundingModeIndependence<<"\n";
  o<<"HPR_CORPUS|64 bounded finite triples per position, near half rounding boundaries and cancellation. Baseline and perturb_selected_lane0 phases.\n";
  o<<"HPR_MODELS_NOTE|24 shader-level models include early/late conversion, FP32 output RTE/RTZ and mixed input/product/add rounding.\n";
  o<<"HPR_POSITION_NOTE|one FP32 source in GLSL A/B/C; other sources are exact half values. Driver may commute or materialize conversions. Read emitted ISA.\n";
  o<<"HPR_MATCHED_ROUNDING|extra controls RTZ16_RTZ32 and RTE16_RTZ32; compare emitted arithmetic FP_MODE, not only the requested modes. Compiler may insert s_round_mode.\n";
  const char* operations[]={"Fma half","mul-add half","Fma promoted keep FP32","split half NoContraction"};
  const char* modes[]={"default","RTE16_RTE32","RTZ16_RTE32","RTZ16_RTZ32","RTE16_RTZ32"};
  for(uint32_t requested:{0u,32u,64u})for(int mode=0;mode<5;mode++)for(int kind=0;kind<12;kind++){
   bool w=!requested||(sizeAvailable&&sz.subgroupSizeControl&&(sp.requiredSubgroupSizeStages&VK_SHADER_STAGE_COMPUTE_BIT)&&requested>=sp.minSubgroupSize&&requested<=sp.maxSubgroupSize&&64/requested<=sp.maxComputeWorkgroupSubgroups);
   bool rounding=hp_roles::modeSupported(mode,fc.shaderRoundingModeRTEFloat16,fc.shaderRoundingModeRTZFloat16,
    fc.shaderRoundingModeRTEFloat32,fc.shaderRoundingModeRTZFloat32,fc.roundingModeIndependence!=VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_NONE);
   bool basic=(sg.supportedStages&VK_SHADER_STAGE_COMPUTE_BIT)&&(sg.supportedOperations&VK_SUBGROUP_FEATURE_BASIC_BIT);
   std::string label=std::string("HPR FP32_")+char('A'+kind/4)+" "+operations[kind%4]+" "+modes[mode]+" wave="+(requested?std::to_string(requested):"default");
   run(label.c_str(),69+mode*12+kind,basic&&f12.shaderFloat16&&rounding&&w&&(requested||sg.subgroupSize==32||sg.subgroupSize==64),requested);
  }
  continue;
 }
 if(hpoOnly){
  o<<"HPO_BUILD|"<<fma_compiler_metadata<<"\n";
  o<<"HPO_FEATURES|Float16="<<f12.shaderFloat16<<"|RTE16="<<fc.shaderRoundingModeRTEFloat16<<"|RTZ16="<<fc.shaderRoundingModeRTZFloat16<<"|RTE32="<<fc.shaderRoundingModeRTEFloat32<<"|rounding_independence="<<fc.roundingModeIndependence<<"\n";
  o<<"HPO_CORPUS|64 finite FP32 inputs, varying signed half coefficients and cancellation; two phases: varying_coefficients and matched_sources. Zero signs equivalent; no NaN/Inf/subnormal claims.\n";
  o<<"HPO_SOURCES|uniform B/C push constants versus per-lane buffers; allocation must be read from emitted ISA. Matched phase uses B=0x4c01 C=0xdc02 on every lane. Upper/lower lanes carry independent halves in phase one.\n";
  o<<"HPO_MODELS_NOTE|24 models include half/full-FP32 outputs, RTE32/RTZ32 and independent input/product/add rounding; default mode is observational.\n";
  const char* kinds[]={"Fma half per-lane lower","Fma half uniform push","Fma half negate A","Fma half abs A negate C","mul-add half per-lane","mul-add half uniform push","Fma promoted half keep FP32","split half NoContraction","Fma half per-lane upper"};
  const char* modes[]={"default","RTE16_RTE32","RTZ16_RTE32"};
  for(uint32_t requested:{0u,32u,64u})for(int mode=0;mode<3;mode++)for(int kind=0;kind<9;kind++){
   bool w=!requested||(sizeAvailable&&sz.subgroupSizeControl&&(sp.requiredSubgroupSizeStages&VK_SHADER_STAGE_COMPUTE_BIT)&&requested>=sp.minSubgroupSize&&requested<=sp.maxSubgroupSize&&64/requested<=sp.maxComputeWorkgroupSubgroups);
   bool rounding=mode==0||(fc.shaderRoundingModeRTEFloat32&&(mode==1?fc.shaderRoundingModeRTEFloat16:(fc.shaderRoundingModeRTZFloat16&&fc.roundingModeIndependence!=VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_NONE)));
   bool basic=(sg.supportedStages&VK_SHADER_STAGE_COMPUTE_BIT)&&(sg.supportedOperations&VK_SUBGROUP_FEATURE_BASIC_BIT);
   std::string label=std::string("HPO ")+kinds[kind]+" "+modes[mode]+" wave="+(requested?std::to_string(requested):"default");
   run(label.c_str(),42+mode*9+kind,basic&&f12.shaderFloat16&&rounding&&w&&(requested||sg.subgroupSize==32||sg.subgroupSize==64),requested);
  }
  continue;
 }
 if(hpOnly){
  o<<"HP_BUILD|"<<fma_compiler_metadata<<"\n";
  o<<"HP_FEATURES|Float16="<<f12.shaderFloat16<<"|RTE16="<<fc.shaderRoundingModeRTEFloat16<<"|RTZ16="<<fc.shaderRoundingModeRTZFloat16<<"|RTE32="<<fc.shaderRoundingModeRTEFloat32<<"|rounding_independence="<<fc.roundingModeIndependence<<"\n";
  o<<"Corpus: uint inputs including 1..8 and half boundaries; FP32 inputs near half ties, positive/negative. Constants B=1.5 C=0.5, including the buffer-operand path. No NaN/Inf/subnormal result claims.\n";
  o<<"Explicit modes request RTE32 plus RTE16 or RTZ16. Default-mode numerical models assume RTE32 and are observational only.\n";
  const char* kinds[]={"uint-to-half mul-add constants","FP32-to-half mul-add constants","FP32-to-half Fma constants","FP32 Fma then half","FP32-to-half split NoContraction","FP32-to-half Fma buffer operands"};
  const char* modes[]={"default","RTE16_RTE32","RTZ16_RTE32"};
  for(uint32_t requested:{0u,32u,64u})for(int mode=0;mode<3;mode++)for(int kind=0;kind<6;kind++){
   bool w=!requested||(sizeAvailable&&sz.subgroupSizeControl&&(sp.requiredSubgroupSizeStages&VK_SHADER_STAGE_COMPUTE_BIT)&&requested>=sp.minSubgroupSize&&requested<=sp.maxSubgroupSize&&64/requested<=sp.maxComputeWorkgroupSubgroups);
   bool rounding=mode==0||(fc.shaderRoundingModeRTEFloat32&&(mode==1?fc.shaderRoundingModeRTEFloat16:(fc.shaderRoundingModeRTZFloat16&&fc.roundingModeIndependence!=VK_SHADER_FLOAT_CONTROLS_INDEPENDENCE_NONE)));
   bool basic=(sg.supportedStages&VK_SHADER_STAGE_COMPUTE_BIT)&&(sg.supportedOperations&VK_SUBGROUP_FEATURE_BASIC_BIT);
   std::string label=std::string("HP ")+kinds[kind]+" "+modes[mode]+" wave="+(requested?std::to_string(requested):"default");
   run(label.c_str(),24+mode*6+kind,basic&&f12.shaderFloat16&&rounding&&w&&(requested||sg.subgroupSize==32||sg.subgroupSize==64),requested);
  }
  continue;
 }
 if(fmaOnly){
  o<<"FMA_BUILD|"<<fma_compiler_metadata<<"\n";
  o<<"FMA_FEATURES|shaderFloat16="<<f12.shaderFloat16<<"|RTE16="<<fc.shaderRoundingModeRTEFloat16<<"|RTZ16="<<fc.shaderRoundingModeRTZFloat16<<"|default_subgroup="<<sg.subgroupSize<<"|rounding_independence="<<fc.roundingModeIndependence<<"\n";
  o<<"Corpus: 128 finite half triples, independent upper/lower halves; normal inputs/intermediates/results or zero. Zero sign ignored. No NaN/Inf/subnormal claims.\n";
  o<<"GLSL.std.450 Fma is observational: fused/split model matches are not conformance guarantees. Explicit-rounding split controls use NoContraction.\n";
  const char* kinds[]={"scalar Fma","packed Fma","split NoContraction"};const char* modes[]={"default","RTE16","RTZ16"};
  for(uint32_t requested:{0u,32u,64u})for(int mode=0;mode<3;mode++)for(int kind=0;kind<3;kind++){
   bool w=!requested||(sizeAvailable&&sz.subgroupSizeControl&&(sp.requiredSubgroupSizeStages&VK_SHADER_STAGE_COMPUTE_BIT)&&requested>=sp.minSubgroupSize&&requested<=sp.maxSubgroupSize&&64/requested<=sp.maxComputeWorkgroupSubgroups);
   bool rounding=mode==0||(mode==1?fc.shaderRoundingModeRTEFloat16:fc.shaderRoundingModeRTZFloat16);
   bool basic=(sg.supportedStages&VK_SHADER_STAGE_COMPUTE_BIT)&&(sg.supportedOperations&VK_SUBGROUP_FEATURE_BASIC_BIT);
   std::string label=std::string("FMA ")+kinds[kind]+" "+modes[mode]+" wave="+(requested?std::to_string(requested):"default");
   run(label.c_str(),15+mode*3+kind,basic&&f12.shaderFloat16&&rounding&&w&&(requested||sg.subgroupSize==32||sg.subgroupSize==64),requested);
  }
  continue;
 }
 run("UINT32 baseline",0,true);run("Subgroup arithmetic default",1,sub);run("FP16 arithmetic",2,f12.shaderFloat16);run("INT8 arithmetic",3,f12.shaderInt8);
 run("LDS shared-memory exchange",5,true);
 if(!has_probe9)o<<"\n[TEST] ALU/subgroup 4x4 matrix | SKIP build-time shader unavailable\n";else run("ALU/subgroup 4x4 matrix",9,(sg.supportedStages&VK_SHADER_STAGE_COMPUTE_BIT)&&(sg.supportedOperations&VK_SUBGROUP_FEATURE_BASIC_BIT)&&(sg.supportedOperations&VK_SUBGROUP_FEATURE_SHUFFLE_BIT)&&(sg.subgroupSize==32||sg.subgroupSize==64));
 if(!has_probe6)o<<"\n[TEST] INT8 SDot | SKIP build-time SPIR-V unavailable\n";else run("INT8 SDot",6,dotAvailable&&dot.shaderIntegerDotProduct&&f12.shaderInt8);
 if(!has_probe7)o<<"\n[TEST] INT16 SDot | SKIP build-time SPIR-V unavailable\n";else run("INT16 SDot",7,dotAvailable&&dot.shaderIntegerDotProduct&&ff.features.shaderInt16);
 if(!has_probe8)o<<"\n[TEST] FP32 buffer atomic add | SKIP build-time shader unavailable\n";else run("FP32 buffer atomic add",8,atomicAvailable&&af.shaderBufferFloat32AtomicAdd);
 bool shape=false,signedShape=false,unsignedShape=false;if(matrix&&cm.cooperativeMatrix){auto fn=(PFN_vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR)vkGetInstanceProcAddr(ins.i,"vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR");uint32_t m=0;if(fn&&fn(p,&m,nullptr)==VK_SUCCESS&&m<4096){std::vector<VkCooperativeMatrixPropertiesKHR> v(m);for(auto& x:v)x.sType=VK_STRUCTURE_TYPE_COOPERATIVE_MATRIX_PROPERTIES_KHR;if(fn(p,&m,v.data())==VK_SUCCESS)for(auto& x:v)if(x.MSize==16&&x.NSize==16&&x.KSize==16&&x.scope==VK_SCOPE_SUBGROUP_KHR&&!x.saturatingAccumulation){if(x.AType==VK_COMPONENT_TYPE_FLOAT16_KHR&&x.BType==VK_COMPONENT_TYPE_FLOAT16_KHR&&x.CType==VK_COMPONENT_TYPE_FLOAT32_KHR&&x.ResultType==VK_COMPONENT_TYPE_FLOAT32_KHR)shape=true;if(x.AType==VK_COMPONENT_TYPE_SINT8_KHR&&x.BType==VK_COMPONENT_TYPE_SINT8_KHR&&x.CType==VK_COMPONENT_TYPE_SINT32_KHR&&x.ResultType==VK_COMPONENT_TYPE_SINT32_KHR)signedShape=true;if(x.AType==VK_COMPONENT_TYPE_UINT8_KHR&&x.BType==VK_COMPONENT_TYPE_UINT8_KHR&&x.CType==VK_COMPONENT_TYPE_UINT32_KHR&&x.ResultType==VK_COMPONENT_TYPE_UINT32_KHR)unsignedShape=true;}}}
 if(!has_probe4)o<<"\n[TEST] KHR cooperative matrix 16x16 FP16/FP32 | SKIP build-time shader compiler does not support this shader; hardware support not determined\n";else run("KHR cooperative matrix 16x16 FP16/FP32",4,shape&&f12.shaderFloat16&&f12.vulkanMemoryModel&&(cmp.cooperativeMatrixSupportedStages&VK_SHADER_STAGE_COMPUTE_BIT)&&(sg.subgroupSize==32||sg.subgroupSize==64));
 bool matCommon=matrix&&cm.cooperativeMatrix&&f12.vulkanMemoryModel&&(cmp.cooperativeMatrixSupportedStages&VK_SHADER_STAGE_COMPUTE_BIT)&&(sg.supportedStages&VK_SHADER_STAGE_COMPUTE_BIT)&&(sg.supportedOperations&VK_SUBGROUP_FEATURE_BASIC_BIT);
 auto runMatrix=[&](int id,const char* label,bool built,bool supported,uint32_t wave){if(!built){o<<"\n[TEST] "<<label<<" | SKIP build-time SPIR-V unavailable\n";return;}run(label,id,matCommon&&supported,wave);};
 for(uint32_t wave:{32u,64u}){bool w=sizeAvailable&&sz.subgroupSizeControl&&(sp.requiredSubgroupSizeStages&VK_SHADER_STAGE_COMPUTE_BIT)&&wave>=sp.minSubgroupSize&&wave<=sp.maxSubgroupSize&&64/wave<=sp.maxComputeWorkgroupSubgroups;
 run(wave==32?"Wave32 requested":"Wave64 requested",1,sub&&w,wave);
 if(has_probe10)run(wave==32?"Packed FP16 FMA wave32":"Packed FP16 FMA wave64",10,w&&f12.shaderFloat16,wave);else o<<"\n[TEST] Packed FP16 FMA | SKIP compiler unavailable\n";
 if(has_probe11)run(wave==32?"Scalar/vector wave32":"Scalar/vector wave64",11,w&&(sg.supportedStages&VK_SHADER_STAGE_COMPUTE_BIT)&&(sg.supportedOperations&VK_SUBGROUP_FEATURE_BASIC_BIT)&&(sg.supportedOperations&VK_SUBGROUP_FEATURE_BALLOT_BIT),wave);
 runMatrix(12,wave==32?"SPIR-V matrix FP16/FP32 wave32":"SPIR-V matrix FP16/FP32 wave64",has_probe12,w&&shape&&f12.shaderFloat16,wave);
 runMatrix(13,wave==32?"SPIR-V matrix SINT8/I32 wave32":"SPIR-V matrix SINT8/I32 wave64",has_probe13,w&&signedShape&&f12.shaderInt8,wave);
 runMatrix(14,wave==32?"SPIR-V matrix UINT8/U32 wave32":"SPIR-V matrix UINT8/U32 wave64",has_probe14,w&&unsignedShape&&f12.shaderInt8,wave);
 }
 if(sg.subgroupSize==32||sg.subgroupSize==64){runMatrix(12,"SPIR-V matrix FP16/FP32 default",has_probe12,shape&&f12.shaderFloat16,0);runMatrix(13,"SPIR-V matrix SINT8/I32 default",has_probe13,signedShape&&f12.shaderInt8,0);runMatrix(14,"SPIR-V matrix UINT8/U32 default",has_probe14,unsignedShape&&f12.shaderInt8,0);}
 }
 }catch(const std::exception& e){o<<"INCONCLUSIVE "<<e.what()<<"\n";}
 o<<(hprOnly?"HPR source-role research complete. Check actual operand position and precision in ISA.\n":hpoOnly?"HPO research complete. Correlate operand paths, FP32 models and actual emitted ISA.\n":hpOnly?"HP research complete. Check per-shader hp emission before interpreting numeric observations.\n":fmaOnly?"FMA research complete. Correlate shader-level observations with emitted ISA; no WMMA conclusion.\n":"PASS validates shader results via active driver. ALU/subgroup matrix is a portable fallback, not native WMMA; a skipped feature is not evidence of absent silicon. Timeout terminates probe process without modifying system settings.\n");return o.str();
}

std::string compute_scan(){return compute_scan_impl(0);}
std::string fma_scan(){return compute_scan_impl(1);}
std::string hp_scan(){return compute_scan_impl(2);}

std::string hp_operand_scan(){return compute_scan_impl(3);}

std::string hp_role_scan(){return compute_scan_impl(4);}
