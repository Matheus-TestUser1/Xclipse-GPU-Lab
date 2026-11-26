#include <vulkan/vulkan.h>
#include "compute_generated.inc"
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
 VkDevice d{};VkBuffer b{};VkDeviceMemory mem{};VkDescriptorSetLayout sl{};VkPipelineLayout pl{};VkShaderModule sm{};VkPipeline pipe{};VkDescriptorPool pool{};VkCommandPool cp{};VkFence fence{};void* map{};
 ~Run(){if(!d)return;if(map)vkUnmapMemory(d,mem);if(fence)vkDestroyFence(d,fence,nullptr);if(cp)vkDestroyCommandPool(d,cp,nullptr);if(pool)vkDestroyDescriptorPool(d,pool,nullptr);if(pipe)vkDestroyPipeline(d,pipe,nullptr);if(sm)vkDestroyShaderModule(d,sm,nullptr);if(pl)vkDestroyPipelineLayout(d,pl,nullptr);if(sl)vkDestroyDescriptorSetLayout(d,sl,nullptr);if(b)vkDestroyBuffer(d,b,nullptr);if(mem)vkFreeMemory(d,mem,nullptr);vkDestroyDevice(d,nullptr);}
};
void test(std::ostream& o,VkPhysicalDevice p,uint32_t family,int id,uint32_t wave,bool useExt){
 Run r;float pri=1;VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};qi.queueFamilyIndex=family;qi.queueCount=1;qi.pQueuePriorities=&pri;
 VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};f12.shaderFloat16=id==2;f12.shaderInt8=id==3;
 VkPhysicalDeviceSubgroupSizeControlFeatures sz{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES};sz.subgroupSizeControl=wave!=0;
 if(wave)f12.pNext=&sz;
 VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};di.pNext=&f12;di.queueCreateInfoCount=1;di.pQueueCreateInfos=&qi;
 const char* ext=VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME;if(wave&&useExt){di.enabledExtensionCount=1;di.ppEnabledExtensionNames=&ext;}
 check(vkCreateDevice(p,&di,nullptr,&r.d),"device");VkQueue q;vkGetDeviceQueue(r.d,family,0,&q);
 VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};bi.size=128*sizeof(uint32_t);bi.usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;bi.sharingMode=VK_SHARING_MODE_EXCLUSIVE;check(vkCreateBuffer(r.d,&bi,nullptr,&r.b),"buffer");
 VkMemoryRequirements req;vkGetBufferMemoryRequirements(r.d,r.b,&req);VkPhysicalDeviceMemoryProperties mp;vkGetPhysicalDeviceMemoryProperties(p,&mp);uint32_t mt=UINT32_MAX;
 for(uint32_t i=0;i<mp.memoryTypeCount;i++)if((req.memoryTypeBits&(1u<<i))&&((mp.memoryTypes[i].propertyFlags&(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))==(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))){mt=i;break;}
 if(mt==UINT32_MAX)throw std::runtime_error("no host coherent storage memory");
 VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.allocationSize=req.size;ai.memoryTypeIndex=mt;check(vkAllocateMemory(r.d,&ai,nullptr,&r.mem),"allocate");check(vkBindBufferMemory(r.d,r.b,r.mem,0),"bind");check(vkMapMemory(r.d,r.mem,0,VK_WHOLE_SIZE,0,&r.map),"map");
 auto* data=static_cast<uint32_t*>(r.map);for(unsigned i=0;i<64;i++){data[i]=0xffffffffu;data[i+64]=1+i%8;}
 VkDescriptorSetLayoutBinding lb{};lb.binding=0;lb.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;lb.descriptorCount=1;lb.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;VkDescriptorSetLayoutCreateInfo li{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};li.bindingCount=1;li.pBindings=&lb;check(vkCreateDescriptorSetLayout(r.d,&li,nullptr,&r.sl),"descriptor layout");
 VkPipelineLayoutCreateInfo pli{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pli.setLayoutCount=1;pli.pSetLayouts=&r.sl;check(vkCreatePipelineLayout(r.d,&pli,nullptr,&r.pl),"pipeline layout");
 const uint32_t* code=probe0;size_t bytes=sizeof(probe0);if(id==1){code=probe1;bytes=sizeof(probe1);}if(id==2){code=probe2;bytes=sizeof(probe2);}if(id==3){code=probe3;bytes=sizeof(probe3);}
 VkShaderModuleCreateInfo si{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};si.codeSize=bytes;si.pCode=code;check(vkCreateShaderModule(r.d,&si,nullptr,&r.sm),"shader module");
 VkPipelineShaderStageRequiredSubgroupSizeCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO};rs.requiredSubgroupSize=wave;
 VkComputePipelineCreateInfo pi{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};pi.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;pi.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;pi.stage.module=r.sm;pi.stage.pName="main";if(wave)pi.stage.pNext=&rs;pi.layout=r.pl;check(vkCreateComputePipelines(r.d,VK_NULL_HANDLE,1,&pi,nullptr,&r.pipe),"compute pipeline");
 VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1};VkDescriptorPoolCreateInfo dpi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};dpi.maxSets=1;dpi.poolSizeCount=1;dpi.pPoolSizes=&ps;check(vkCreateDescriptorPool(r.d,&dpi,nullptr,&r.pool),"pool");
 VkDescriptorSetAllocateInfo da{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};da.descriptorPool=r.pool;da.descriptorSetCount=1;da.pSetLayouts=&r.sl;VkDescriptorSet set;check(vkAllocateDescriptorSets(r.d,&da,&set),"descriptor");
 VkDescriptorBufferInfo db{r.b,0,VK_WHOLE_SIZE};VkWriteDescriptorSet wr{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};wr.dstSet=set;wr.dstBinding=0;wr.descriptorCount=1;wr.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;wr.pBufferInfo=&db;vkUpdateDescriptorSets(r.d,1,&wr,0,nullptr);
 VkCommandPoolCreateInfo cpi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};cpi.queueFamilyIndex=family;check(vkCreateCommandPool(r.d,&cpi,nullptr,&r.cp),"command pool");VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=r.cp;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;VkCommandBuffer cmd;check(vkAllocateCommandBuffers(r.d,&ca,&cmd),"command buffer");
 VkCommandBufferBeginInfo cb{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};cb.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;check(vkBeginCommandBuffer(cmd,&cb),"begin");
 VkMemoryBarrier before{VK_STRUCTURE_TYPE_MEMORY_BARRIER};before.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT;before.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&before,0,nullptr,0,nullptr);
 vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,r.pipe);vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,r.pl,0,1,&set,0,nullptr);vkCmdDispatch(cmd,1,1,1);
 VkMemoryBarrier after{VK_STRUCTURE_TYPE_MEMORY_BARRIER};after.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;after.dstAccessMask=VK_ACCESS_HOST_READ_BIT;vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&after,0,nullptr,0,nullptr);check(vkEndCommandBuffer(cmd),"end");
 VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};check(vkCreateFence(r.d,&fi,nullptr,&r.fence),"fence");VkSubmitInfo sub{VK_STRUCTURE_TYPE_SUBMIT_INFO};sub.commandBufferCount=1;sub.pCommandBuffers=&cmd;auto start=std::chrono::steady_clock::now();check(vkQueueSubmit(q,1,&sub,r.fence),"submit");VkResult wait=vkWaitForFences(r.d,1,&r.fence,VK_TRUE,5000000000ull);if(wait==VK_TIMEOUT)std::_Exit(124);check(wait,"fence wait");double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
 unsigned mismatch=0;for(unsigned i=0;i<64;i++){uint32_t expected=(1+i%8)*7+3;if(id==3)expected=(1+i%8)*3+1;if(id==2){float v=(1+i%8)*1.5f+0.5f;std::memcpy(&expected,&v,sizeof(v));}if(id==1){uint32_t size=data[i]>>16,sum=data[i]&0xffff;if(size==0||size>128||sum!=std::min(size,64u)||(wave&&size!=wave))mismatch++;}else if(data[i]!=expected)mismatch++;}
 o<<(mismatch?"FAIL":"PASS")<<" mismatch="<<mismatch<<"/64 first=0x"<<std::hex<<data[0]<<std::dec<<" submit_to_fence_ms="<<ms<<"\n";
}
}
std::string compute_scan(){
 std::ostringstream o;o<<"[Functional Vulkan compute tests / known answers]\n";
 try{uint32_t api=VK_API_VERSION_1_0;auto ev=(PFN_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(nullptr,"vkEnumerateInstanceVersion");if(ev)check(ev(&api),"instance version");if(api<VK_API_VERSION_1_2)return "INCONCLUSIVE: loader Vulkan <1.2";VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="GPU Lab compute";app.apiVersion=std::min(api,(uint32_t)VK_API_VERSION_1_3);VkInstanceCreateInfo ii{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ii.pApplicationInfo=&app;Instance ins;check(vkCreateInstance(&ii,nullptr,&ins.i),"instance");uint32_t n=0;check(vkEnumeratePhysicalDevices(ins.i,&n,nullptr),"enumerate");std::vector<VkPhysicalDevice> ds(n);check(vkEnumeratePhysicalDevices(ins.i,&n,ds.data()),"devices");
 for(auto p:ds){VkPhysicalDeviceProperties prop{};vkGetPhysicalDeviceProperties(p,&prop);o<<"\n[GPU] "<<prop.deviceName<<"\n";uint32_t ver=std::min(api,prop.apiVersion);if(ver<VK_API_VERSION_1_2||prop.limits.maxComputeWorkGroupInvocations<64||prop.limits.maxComputeWorkGroupSize[0]<64){o<<"SKIP: API/workgroup limit\n";continue;}
 uint32_t k=0;vkGetPhysicalDeviceQueueFamilyProperties(p,&k,nullptr);std::vector<VkQueueFamilyProperties> qs(k);vkGetPhysicalDeviceQueueFamilyProperties(p,&k,qs.data());auto qi=std::find_if(qs.begin(),qs.end(),[](auto x){return x.queueCount&&(x.queueFlags&VK_QUEUE_COMPUTE_BIT);});if(qi==qs.end()){o<<"SKIP no compute queue\n";continue;}uint32_t family=qi-qs.begin();
 uint32_t ec=0;check(vkEnumerateDeviceExtensionProperties(p,nullptr,&ec,nullptr),"extensions");std::vector<VkExtensionProperties> es(ec);check(vkEnumerateDeviceExtensionProperties(p,nullptr,&ec,es.data()),"extensions");bool ext=std::any_of(es.begin(),es.end(),[](auto& e){return std::strcmp(e.extensionName,VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME)==0;});bool sizeAvailable=ver>=VK_API_VERSION_1_3||ext;
 VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};VkPhysicalDeviceSubgroupSizeControlFeatures sz{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES};VkPhysicalDeviceFeatures2 ff{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};ff.pNext=&f12;if(sizeAvailable)f12.pNext=&sz;vkGetPhysicalDeviceFeatures2(p,&ff);
 VkPhysicalDeviceSubgroupProperties sg{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};VkPhysicalDeviceSubgroupSizeControlProperties sp{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES};VkPhysicalDeviceProperties2 pp{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};pp.pNext=&sg;if(sizeAvailable)sg.pNext=&sp;vkGetPhysicalDeviceProperties2(p,&pp);
 bool sub=(sg.supportedStages&VK_SHADER_STAGE_COMPUTE_BIT)&&(sg.supportedOperations&VK_SUBGROUP_FEATURE_BASIC_BIT)&&(sg.supportedOperations&VK_SUBGROUP_FEATURE_ARITHMETIC_BIT);
 auto run=[&](const char* label,int id,bool supported,uint32_t wave=0){o<<label<<" | ";if(!supported){o<<"SKIP feature not reported or requirements unmet\n";return;}try{test(o,p,family,id,wave,ver<VK_API_VERSION_1_3);}catch(const std::exception& e){o<<"INCONCLUSIVE "<<e.what()<<"\n";}};
 run("UINT32 baseline",0,true);run("Subgroup arithmetic default",1,sub);run("FP16 arithmetic",2,f12.shaderFloat16);run("INT8 arithmetic",3,f12.shaderInt8);
 for(uint32_t wave:{32u,64u})run(wave==32?"Wave32 requested":"Wave64 requested",1,sub&&sizeAvailable&&sz.subgroupSizeControl&&(sp.requiredSubgroupSizeStages&VK_SHADER_STAGE_COMPUTE_BIT)&&wave>=sp.minSubgroupSize&&wave<=sp.maxSubgroupSize&&64/wave<=sp.maxComputeWorkgroupSubgroups,wave);
 }
 }catch(const std::exception& e){o<<"INCONCLUSIVE "<<e.what()<<"\n";}
 o<<"PASS validates shader results via active driver; does not identify emitted ISA or physical WMMA. Timeout terminates probe process without modifying system settings.\n";return o.str();
}
