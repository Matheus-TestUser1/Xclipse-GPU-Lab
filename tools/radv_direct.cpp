// Android ARM64 standalone direct-HAL harness; no system Vulkan loader.
#include <vulkan/vulkan.h>
#include <hardware/hwvulkan.h>
#include <dlfcn.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <cstring>
#include <cstdlib>
namespace direct {
static hwvulkan_device_t* hal=nullptr;
static VkInstance instance=VK_NULL_HANDLE;
static VkDevice device=VK_NULL_HANDLE;
static PFN_vkGetDeviceProcAddr gdpa=nullptr;
template<class P> P ip(const char* n){
 auto p=reinterpret_cast<P>(hal->GetInstanceProcAddr(instance,n));
 if(!p)throw std::runtime_error(std::string("missing instance function ")+n);
 return p;
}
template<class P> P dp(const char* n){
 auto p=reinterpret_cast<P>(gdpa(device,n));
 if(!p)throw std::runtime_error(std::string("missing device function ")+n);
 return p;
}
static PFN_vkVoidFunction VKAPI_CALL getInstance(VkInstance i,const char* n){
 return hal->GetInstanceProcAddr(i,n);
}
static PFN_vkVoidFunction VKAPI_CALL getDevice(VkDevice d,const char* n){return gdpa(d,n);}
static VkResult VKAPI_CALL createInstance(const VkInstanceCreateInfo* ci,const VkAllocationCallbacks* a,VkInstance* out){
 auto r=hal->CreateInstance(ci,a,out);
 if(r!=VK_SUCCESS)return r;
 instance=*out;
 try{
  gdpa=ip<PFN_vkGetDeviceProcAddr>("vkGetDeviceProcAddr");
  for(auto n: {"vkEnumeratePhysicalDevices","vkGetPhysicalDeviceProperties","vkGetPhysicalDeviceMemoryProperties","vkGetPhysicalDeviceQueueFamilyProperties","vkEnumerateDeviceExtensionProperties","vkGetPhysicalDeviceFeatures2","vkGetPhysicalDeviceProperties2"})ip<PFN_vkVoidFunction>(n);
 }catch(...){
  auto destroy=reinterpret_cast<PFN_vkDestroyInstance>(hal->GetInstanceProcAddr(instance,"vkDestroyInstance"));
  if(destroy)destroy(instance,a);
  instance=*out=VK_NULL_HANDLE;throw;
 }
 return r;
}
static void VKAPI_CALL destroyInstance(VkInstance i,const VkAllocationCallbacks* a){
 auto p=reinterpret_cast<PFN_vkDestroyInstance>(hal->GetInstanceProcAddr(i,"vkDestroyInstance"));
 if(p)p(i,a);instance=VK_NULL_HANDLE;gdpa=nullptr;
}
static VkResult VKAPI_CALL createDevice(VkPhysicalDevice p,const VkDeviceCreateInfo* ci,const VkAllocationCallbacks* a,VkDevice* out){
 auto r=ip<PFN_vkCreateDevice>("vkCreateDevice")(p,ci,a,out);
 if(r!=VK_SUCCESS)return r;
 device=*out;
 try{for(auto n:{"vkUnmapMemory","vkDestroyQueryPool","vkDestroyFence","vkDestroyCommandPool","vkDestroyDescriptorPool","vkDestroyPipeline","vkDestroyShaderModule","vkDestroyPipelineLayout","vkDestroyDescriptorSetLayout","vkDestroyBuffer","vkFreeMemory","vkGetDeviceQueue","vkCreateBuffer","vkGetBufferMemoryRequirements","vkAllocateMemory","vkBindBufferMemory","vkMapMemory","vkCreateDescriptorSetLayout","vkCreatePipelineLayout","vkCreateShaderModule","vkCreateComputePipelines","vkCreateDescriptorPool","vkAllocateDescriptorSets","vkUpdateDescriptorSets","vkCreateCommandPool","vkAllocateCommandBuffers","vkCreateQueryPool","vkBeginCommandBuffer","vkCmdResetQueryPool","vkCmdWriteTimestamp","vkCmdPipelineBarrier","vkCmdBindPipeline","vkCmdBindDescriptorSets","vkCmdPushConstants","vkCmdDispatch","vkEndCommandBuffer","vkCreateFence","vkQueueSubmit","vkWaitForFences","vkResetFences","vkGetQueryPoolResults"})dp<PFN_vkVoidFunction>(n);}
 catch(...){
  auto destroy=reinterpret_cast<PFN_vkDestroyDevice>(gdpa(device,"vkDestroyDevice"));
  if(destroy)destroy(device,a);
  device=*out=VK_NULL_HANDLE;throw;
 }
 return r;
}
static void VKAPI_CALL destroyDevice(VkDevice d,const VkAllocationCallbacks* a){
 auto p=reinterpret_cast<PFN_vkDestroyDevice>(gdpa(d,"vkDestroyDevice"));
 if(p)p(d,a);device=VK_NULL_HANDLE;
}
}
#define vkGetInstanceProcAddr direct::getInstance
#define vkGetDeviceProcAddr direct::getDevice
#define vkCreateInstance direct::createInstance
#define vkDestroyInstance direct::destroyInstance
#define vkCreateDevice direct::createDevice
#define vkDestroyDevice direct::destroyDevice
#define vkEnumeratePhysicalDevices direct::ip<PFN_vkEnumeratePhysicalDevices>("vkEnumeratePhysicalDevices")
#define vkGetPhysicalDeviceProperties direct::ip<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties")
#define vkGetPhysicalDeviceMemoryProperties direct::ip<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties")
#define vkGetPhysicalDeviceQueueFamilyProperties direct::ip<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties")
#define vkEnumerateDeviceExtensionProperties direct::ip<PFN_vkEnumerateDeviceExtensionProperties>("vkEnumerateDeviceExtensionProperties")
#define vkGetPhysicalDeviceFeatures2 direct::ip<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")
#define vkGetPhysicalDeviceProperties2 direct::ip<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")
#define vkUnmapMemory direct::dp<PFN_vkUnmapMemory>("vkUnmapMemory")
#define vkDestroyQueryPool direct::dp<PFN_vkDestroyQueryPool>("vkDestroyQueryPool")
#define vkDestroyFence direct::dp<PFN_vkDestroyFence>("vkDestroyFence")
#define vkDestroyCommandPool direct::dp<PFN_vkDestroyCommandPool>("vkDestroyCommandPool")
#define vkDestroyDescriptorPool direct::dp<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")
#define vkDestroyPipeline direct::dp<PFN_vkDestroyPipeline>("vkDestroyPipeline")
#define vkDestroyShaderModule direct::dp<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")
#define vkDestroyPipelineLayout direct::dp<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")
#define vkDestroyDescriptorSetLayout direct::dp<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")
#define vkDestroyBuffer direct::dp<PFN_vkDestroyBuffer>("vkDestroyBuffer")
#define vkFreeMemory direct::dp<PFN_vkFreeMemory>("vkFreeMemory")
#define vkGetDeviceQueue direct::dp<PFN_vkGetDeviceQueue>("vkGetDeviceQueue")
#define vkCreateBuffer direct::dp<PFN_vkCreateBuffer>("vkCreateBuffer")
#define vkGetBufferMemoryRequirements direct::dp<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")
#define vkAllocateMemory direct::dp<PFN_vkAllocateMemory>("vkAllocateMemory")
#define vkBindBufferMemory direct::dp<PFN_vkBindBufferMemory>("vkBindBufferMemory")
#define vkMapMemory direct::dp<PFN_vkMapMemory>("vkMapMemory")
#define vkCreateDescriptorSetLayout direct::dp<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")
#define vkCreatePipelineLayout direct::dp<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")
#define vkCreateShaderModule direct::dp<PFN_vkCreateShaderModule>("vkCreateShaderModule")
#define vkCreateComputePipelines direct::dp<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")
#define vkCreateDescriptorPool direct::dp<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")
#define vkAllocateDescriptorSets direct::dp<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")
#define vkUpdateDescriptorSets direct::dp<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")
#define vkCreateCommandPool direct::dp<PFN_vkCreateCommandPool>("vkCreateCommandPool")
#define vkAllocateCommandBuffers direct::dp<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers")
#define vkCreateQueryPool direct::dp<PFN_vkCreateQueryPool>("vkCreateQueryPool")
#define vkBeginCommandBuffer direct::dp<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")
#define vkCmdResetQueryPool direct::dp<PFN_vkCmdResetQueryPool>("vkCmdResetQueryPool")
#define vkCmdWriteTimestamp direct::dp<PFN_vkCmdWriteTimestamp>("vkCmdWriteTimestamp")
#define vkCmdPipelineBarrier direct::dp<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")
#define vkCmdBindPipeline direct::dp<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")
#define vkCmdBindDescriptorSets direct::dp<PFN_vkCmdBindDescriptorSets>("vkCmdBindDescriptorSets")
#define vkCmdPushConstants direct::dp<PFN_vkCmdPushConstants>("vkCmdPushConstants")
#define vkCmdDispatch direct::dp<PFN_vkCmdDispatch>("vkCmdDispatch")
#define vkEndCommandBuffer direct::dp<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")
#define vkCreateFence direct::dp<PFN_vkCreateFence>("vkCreateFence")
#define vkQueueSubmit direct::dp<PFN_vkQueueSubmit>("vkQueueSubmit")
#define vkWaitForFences direct::dp<PFN_vkWaitForFences>("vkWaitForFences")
#define vkResetFences direct::dp<PFN_vkResetFences>("vkResetFences")
#define vkGetQueryPoolResults direct::dp<PFN_vkGetQueryPoolResults>("vkGetQueryPoolResults")
#include "../app/src/main/cpp/compute.cpp"
int main(int argc,char** argv){
 if(argc!=3||argv[1][0]!='/' ){
  std::cerr<<"Usage: radv-direct /absolute/path/vulkan.radeon.so smoke|matrix|fma|hp|operands|roles|special|bench|dynamic\n";return 2;
 }
 const char* suites[]={"matrix","fma","hp","operands","roles","special","bench","dynamic","smoke"};
 int mode=-1;for(int i=0;i<9;i++)if(!std::strcmp(argv[2],suites[i]))mode=i;
 if(mode<0)return 2;
 setenv("MESA_SHADER_CACHE_DISABLE","1",1);
 std::cout.setf(std::ios::unitbuf);
 std::cout<<"DIRECT_BEGIN|version=0.15|driver="<<argv[1]<<"|suite="<<argv[2]<<"\n";
 for(const char* name:{"RADV_XCLIPSE_COOPMAT_ALU","RADV_DEBUG","RADV_PERFTEST","MESA_SHADER_CACHE_DISABLE"})
  std::cout<<"DIRECT_ENV|"<<name<<"="<<(getenv(name)?getenv(name):"<unset>")<<"\n";
 void* lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
 if(!lib){std::cerr<<"DIRECT_FAIL|dlopen|"<<dlerror()<<"\n";return 3;}
 auto* module=reinterpret_cast<hwvulkan_module_t*>(dlsym(lib,HAL_MODULE_INFO_SYM_AS_STR));
 if(!module||!module->common.methods||!module->common.methods->open){std::cerr<<"DIRECT_FAIL|HAL module missing\n";return 4;}
 hw_device_t* base=nullptr;int rc=module->common.methods->open(&module->common,HWVULKAN_DEVICE_0,&base);
 if(rc||!base){std::cerr<<"DIRECT_FAIL|HAL open="<<rc<<"\n";return 5;}
 direct::hal=reinterpret_cast<hwvulkan_device_t*>(base);
 if(!direct::hal->CreateInstance||!direct::hal->GetInstanceProcAddr){std::cerr<<"DIRECT_FAIL|HAL entrypoints missing\n";return 6;}
 int status=0;
 try{auto result=compute_scan_impl(mode,&std::cout,true,&status);if(!result.empty())std::cout<<result<<"\n";}
 catch(const std::exception& e){std::cerr<<"DIRECT_FAIL|"<<e.what()<<"\n";status=1;}
 std::cout<<"DIRECT_END|status="<<status<<"|execution_not_conformance=1\n";
 if(base->close)base->close(base);
 return status;
}
