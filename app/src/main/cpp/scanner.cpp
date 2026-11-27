#include <jni.h>
#include <vulkan/vulkan.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <dlfcn.h>
#include <algorithm>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdint>
#include <iomanip>
#include <functional>
#include <type_traits>
#include <fstream>
#include <sys/stat.h>
using namespace std;
static jbyteArray result(JNIEnv* e,const string& s){auto a=e->NewByteArray(s.size());if(a&&!s.empty())e->SetByteArrayRegion(a,0,s.size(),reinterpret_cast<const jbyte*>(s.data()));return a;}
static string version(uint32_t v){return to_string(VK_VERSION_MAJOR(v))+"."+to_string(VK_VERSION_MINOR(v))+"."+to_string(VK_VERSION_PATCH(v));}
static string fromjava(JNIEnv* e,jstring j){if(!j)return {};const char* p=e->GetStringUTFChars(j,nullptr);if(!p)return {};string s(p);e->ReleaseStringUTFChars(j,p);return s;}
#define FIELD(x) o<<#x<<"="<<+x<<"\n"
#define F(s,x) o<<#x<<"="<<+(s.x)<<"\n"
template<class T> static void registry_field(ostream& o,const char* name,const T& v){
 o<<name<<"=";
 if constexpr(is_enum_v<T>) o<<static_cast<int64_t>(v);
 else if constexpr(is_arithmetic_v<T>) o<<+v;
 else o<<"[aggregate: not decoded]";
 o<<"\n";
}
template<class T,size_t N> static void registry_field(ostream& o,const char* name,const T (&v)[N]){
 o<<name<<"="; if constexpr(is_same_v<T,char>){for(size_t i=0;i<N&&v[i];i++)o<<v[i];}
 else if constexpr(is_arithmetic_v<T>||is_enum_v<T>){for(size_t i=0;i<N;i++){if(i)o<<",";if constexpr(is_enum_v<T>)o<<static_cast<int64_t>(v[i]);else o<<+v[i];}}
 else o<<"[aggregate array: not decoded]"; o<<"\n";
}
#include "registry_generated.inc"
static string vulkan(const string& probe){
 ostringstream o; uint32_t api=VK_API_VERSION_1_0;auto ev=(PFN_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(nullptr,"vkEnumerateInstanceVersion");if(ev)ev(&api);
 o<<"Loader Vulkan "<<version(api)<<"\n";uint32_t n=0;vkEnumerateInstanceExtensionProperties(nullptr,&n,nullptr);vector<VkExtensionProperties> ie(n);if(n)vkEnumerateInstanceExtensionProperties(nullptr,&n,ie.data());o<<"\n[Instance extensions]\n";for(auto& x:ie)o<<x.extensionName<<" r"<<x.specVersion<<"\n";
 n=0;vkEnumerateInstanceLayerProperties(&n,nullptr);vector<VkLayerProperties> layers(n);if(n)vkEnumerateInstanceLayerProperties(&n,layers.data());o<<"\n[Layers]\n";for(auto& x:layers)o<<x.layerName<<" "<<x.description<<"\n";
 VkApplicationInfo ai{VK_STRUCTURE_TYPE_APPLICATION_INFO};ai.pApplicationName="Xclipse GPU Lab";ai.apiVersion=min(api,(uint32_t)VK_API_VERSION_1_4);VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ci.pApplicationInfo=&ai;VkInstance inst{};auto r=vkCreateInstance(&ci,nullptr,&inst);if(r!=VK_SUCCESS){o<<"vkCreateInstance: "<<r;return o.str();}struct Guard{VkInstance i;~Guard(){vkDestroyInstance(i,nullptr);}}guard{inst};
 n=0;r=vkEnumeratePhysicalDevices(inst,&n,nullptr);if(r!=VK_SUCCESS||!n){o<<"Sem dispositivo: "<<r;return o.str();}vector<VkPhysicalDevice> ds(n);r=vkEnumeratePhysicalDevices(inst,&n,ds.data());if(r!=VK_SUCCESS)return "Falha ao enumerar GPUs";ds.resize(n);
 for(auto d:ds){VkPhysicalDeviceProperties p{};vkGetPhysicalDeviceProperties(d,&p);auto a=min(api,p.apiVersion);o<<"\n[GPU] "<<p.deviceName<<"\nAPI="<<version(p.apiVersion)<<"\nVendorID=0x"<<hex<<p.vendorID<<" DeviceID=0x"<<p.deviceID<<dec<<" driverRaw="<<p.driverVersion<<" type="<<p.deviceType<<"\n";
 uint32_t c=0;vkEnumerateDeviceExtensionProperties(d,nullptr,&c,nullptr);vector<VkExtensionProperties> ex(c);if(c)vkEnumerateDeviceExtensionProperties(d,nullptr,&c,ex.data());auto has=[&](const char* name){return any_of(ex.begin(),ex.end(),[&](auto& x){return strcmp(x.extensionName,name)==0;});};
 o<<"\n[Device extensions: anunciadas]\n";for(auto& x:ex)o<<x.extensionName<<" r"<<x.specVersion<<"\n";
 c=0;vkGetPhysicalDeviceQueueFamilyProperties(d,&c,nullptr);vector<VkQueueFamilyProperties> q(c);if(c)vkGetPhysicalDeviceQueueFamilyProperties(d,&c,q.data());o<<"\n[Queues]\n";for(size_t i=0;i<q.size();i++)o<<i<<" flags=0x"<<hex<<q[i].queueFlags<<dec<<" count="<<q[i].queueCount<<" timestampBits="<<q[i].timestampValidBits<<"\n";
 if(!probe.empty()){
  // Only request advertised extensions. Unknown names are evidence of non-advertisement, not a bypass.
  o<<"\n[Teste de habilitacao] "<<probe<<"\n";
  if(!has(probe.c_str())){o<<"NAO ANUNCIADA: nenhuma chamada invalida executada.\n";continue;}
  if(a<VK_API_VERSION_1_2 || (probe!="VK_EXT_memory_budget" && probe!="VK_KHR_shader_float16_int8" && probe!="VK_KHR_shader_integer_dot_product" && probe!="VK_EXT_subgroup_size_control")){o<<"ANUNCIADA: teste de criacao nao executado (dependencias nao resolvidas nesta versao).\n";continue;}
  auto it=find_if(q.begin(),q.end(),[](auto& x){return x.queueCount&&(x.queueFlags&VK_QUEUE_COMPUTE_BIT);});if(it==q.end())it=find_if(q.begin(),q.end(),[](auto& x){return x.queueCount>0;});if(it==q.end()){o<<"Sem fila\n";continue;}
  float priority=1;VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};qi.queueFamilyIndex=it-q.begin();qi.queueCount=1;qi.pQueuePriorities=&priority;const char* name=probe.c_str();VkDeviceCreateInfo dc{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};dc.queueCreateInfoCount=1;dc.pQueueCreateInfos=&qi;dc.enabledExtensionCount=1;dc.ppEnabledExtensionNames=&name;VkDevice dev{};r=vkCreateDevice(d,&dc,nullptr,&dev);o<<"vkCreateDevice="<<r<<"\n";if(r==VK_SUCCESS){o<<"ACEITA com este conjunto de configuracoes; features especificas e funcionamento ainda nao testados.\n";vkDestroyDevice(dev,nullptr);}else o<<"REJEITADA nesta configuracao; dependencias/features podem faltar.\n";continue;
 }
 o<<"\n[Core limits]\n";F(p.limits,maxImageDimension2D);F(p.limits,maxImageDimension3D);F(p.limits,maxStorageBufferRange);F(p.limits,maxUniformBufferRange);F(p.limits,maxComputeSharedMemorySize);F(p.limits,maxComputeWorkGroupInvocations);F(p.limits,maxBoundDescriptorSets);F(p.limits,maxPushConstantsSize);F(p.limits,timestampPeriod);for(int i=0;i<3;i++)o<<"workgroupSize["<<i<<"]="<<p.limits.maxComputeWorkGroupSize[i]<<" count="<<p.limits.maxComputeWorkGroupCount[i]<<"\n";
 VkPhysicalDeviceFeatures f{};vkGetPhysicalDeviceFeatures(d,&f);o<<"\n[Core features]\n";F(f,shaderFloat64);F(f,shaderInt64);F(f,shaderInt16);F(f,geometryShader);F(f,tessellationShader);F(f,sparseBinding);F(f,shaderStorageImageExtendedFormats);F(f,shaderStorageImageReadWithoutFormat);F(f,shaderStorageImageWriteWithoutFormat);F(f,multiDrawIndirect);F(f,textureCompressionASTC_LDR);F(f,textureCompressionBC);F(f,textureCompressionETC2);
 VkPhysicalDeviceMemoryProperties m{};vkGetPhysicalDeviceMemoryProperties(d,&m);o<<"\n[Memory]\n";for(uint32_t i=0;i<m.memoryHeapCount;i++)o<<"heap "<<i<<" bytes="<<m.memoryHeaps[i].size<<" flags="<<m.memoryHeaps[i].flags<<"\n";for(uint32_t i=0;i<m.memoryTypeCount;i++)o<<"type "<<i<<" heap="<<m.memoryTypes[i].heapIndex<<" flags=0x"<<hex<<m.memoryTypes[i].propertyFlags<<dec<<"\n";
 if(a>=VK_API_VERSION_1_1){
 #define PROP(T,S,V) T V{S};{VkPhysicalDeviceProperties2 base{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};base.pNext=&V;vkGetPhysicalDeviceProperties2(d,&base);}
 #define FEAT(T,S,V) T V{S};{VkPhysicalDeviceFeatures2 base{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};base.pNext=&V;vkGetPhysicalDeviceFeatures2(d,&base);}
 o<<"\n[Subgroup / shader]\n";PROP(VkPhysicalDeviceSubgroupProperties,VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES,sg);F(sg,subgroupSize);F(sg,supportedStages);F(sg,supportedOperations);F(sg,quadOperationsInAllStages);
 if(a>=VK_API_VERSION_1_2||has("VK_KHR_driver_properties")){PROP(VkPhysicalDeviceDriverProperties,VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES,dr);o<<"driverID="<<dr.driverID<<" name="<<dr.driverName<<" info="<<dr.driverInfo<<"\n";}
 if(a>=VK_API_VERSION_1_2||has("VK_KHR_shader_float16_int8")){FEAT(VkPhysicalDeviceShaderFloat16Int8Features,VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES,h);F(h,shaderFloat16);F(h,shaderInt8);}
 if(a>=VK_API_VERSION_1_3||has("VK_EXT_subgroup_size_control")){PROP(VkPhysicalDeviceSubgroupSizeControlProperties,VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES,s);F(s,minSubgroupSize);F(s,maxSubgroupSize);F(s,maxComputeWorkgroupSubgroups);F(s,requiredSubgroupSizeStages);FEAT(VkPhysicalDeviceSubgroupSizeControlFeatures,VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES,sf);F(sf,subgroupSizeControl);F(sf,computeFullSubgroups);}
 if(a>=VK_API_VERSION_1_3||has("VK_KHR_shader_integer_dot_product")){FEAT(VkPhysicalDeviceShaderIntegerDotProductFeatures,VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INTEGER_DOT_PRODUCT_FEATURES,df);F(df,shaderIntegerDotProduct);PROP(VkPhysicalDeviceShaderIntegerDotProductProperties,VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INTEGER_DOT_PRODUCT_PROPERTIES,dp);F(dp,integerDotProduct8BitUnsignedAccelerated);F(dp,integerDotProduct8BitSignedAccelerated);F(dp,integerDotProduct4x8BitPackedUnsignedAccelerated);F(dp,integerDotProduct16BitUnsignedAccelerated);}
 if(has("VK_AMD_shader_core_properties")){PROP(VkPhysicalDeviceShaderCorePropertiesAMD,VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CORE_PROPERTIES_AMD,ac);F(ac,shaderEngineCount);F(ac,shaderArraysPerEngineCount);F(ac,computeUnitsPerShaderArray);F(ac,simdPerComputeUnit);F(ac,wavefrontsPerSimd);F(ac,wavefrontSize);F(ac,sgprsPerSimd);F(ac,vgprsPerSimd);}
 if(has("VK_AMD_shader_core_properties2")){PROP(VkPhysicalDeviceShaderCoreProperties2AMD,VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CORE_PROPERTIES_2_AMD,ac2);F(ac2,shaderCoreFeatures);F(ac2,activeComputeUnitCount);}
 if(has("VK_EXT_memory_budget")){VkPhysicalDeviceMemoryBudgetPropertiesEXT b{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT};VkPhysicalDeviceMemoryProperties2 mp{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2};mp.pNext=&b;vkGetPhysicalDeviceMemoryProperties2(d,&mp);for(uint32_t i=0;i<mp.memoryProperties.memoryHeapCount;i++)o<<"heap "<<i<<" budget="<<b.heapBudget[i]<<" usage="<<b.heapUsage[i]<<" (estimativa do driver para este processo)\n";}
 if(has("VK_KHR_cooperative_matrix")){FEAT(VkPhysicalDeviceCooperativeMatrixFeaturesKHR,VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_FEATURES_KHR,cm);F(cm,cooperativeMatrix);F(cm,cooperativeMatrixRobustBufferAccess);auto fn=(PFN_vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR)vkGetInstanceProcAddr(inst,"vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR");if(fn){uint32_t count=0;if(fn(d,&count,nullptr)==VK_SUCCESS&&count<4096){vector<VkCooperativeMatrixPropertiesKHR> v(count);for(auto& x:v)x.sType=VK_STRUCTURE_TYPE_COOPERATIVE_MATRIX_PROPERTIES_KHR;if(fn(d,&count,v.data())==VK_SUCCESS)for(auto& x:v)o<<"matrix M="<<x.MSize<<" N="<<x.NSize<<" K="<<x.KSize<<" A="<<x.AType<<" B="<<x.BType<<" C="<<x.CType<<" result="<<x.ResultType<<" scope="<<x.scope<<"\n";}}}
 if(has("VK_EXT_mesh_shader")){FEAT(VkPhysicalDeviceMeshShaderFeaturesEXT,VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,mesh);F(mesh,meshShader);F(mesh,taskShader);}
 if(has("VK_KHR_acceleration_structure")){FEAT(VkPhysicalDeviceAccelerationStructureFeaturesKHR,VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,as);F(as,accelerationStructure);}
 if(has("VK_KHR_ray_query")){FEAT(VkPhysicalDeviceRayQueryFeaturesKHR,VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,rq);F(rq,rayQuery);}
 if(has("VK_KHR_ray_tracing_pipeline")){FEAT(VkPhysicalDeviceRayTracingPipelineFeaturesKHR,VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,rt);F(rt,rayTracingPipeline);}
 if(has("VK_KHR_performance_query")){FEAT(VkPhysicalDevicePerformanceQueryFeaturesKHR,VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PERFORMANCE_QUERY_FEATURES_KHR,pq);F(pq,performanceCounterQueryPools);auto fn=(PFN_vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR)vkGetInstanceProcAddr(inst,"vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR");if(fn)for(uint32_t i=0;i<q.size();i++){uint32_t count=0;if(fn(d,i,&count,nullptr,nullptr)==VK_SUCCESS&&count<8192){vector<VkPerformanceCounterKHR> v(count);vector<VkPerformanceCounterDescriptionKHR> desc(count);for(auto& x:v)x.sType=VK_STRUCTURE_TYPE_PERFORMANCE_COUNTER_KHR;for(auto& x:desc)x.sType=VK_STRUCTURE_TYPE_PERFORMANCE_COUNTER_DESCRIPTION_KHR;if(fn(d,i,&count,v.data(),desc.data())==VK_SUCCESS)for(uint32_t j=0;j<count;j++)o<<"counter queue="<<i<<" "<<desc[j].name<<" unit="<<v[j].unit<<" scope="<<v[j].scope<<"\n";}}}
 }
 registry_scan(o,d,a,has);
 o<<"\n[Format support: flags linear/optimal/buffer]\n";for(int fmt=1;fmt<=184;fmt++){VkFormatProperties fp{};vkGetPhysicalDeviceFormatProperties(d,(VkFormat)fmt,&fp);if(fp.linearTilingFeatures||fp.optimalTilingFeatures||fp.bufferFeatures)o<<fmt<<" 0x"<<hex<<fp.linearTilingFeatures<<"/0x"<<fp.optimalTilingFeatures<<"/0x"<<fp.bufferFeatures<<dec<<"\n";}
 o<<"\n[Research candidates: not proof of hardware]\n";for(auto name:{"VK_KHR_cooperative_matrix","VK_NV_cooperative_matrix2","VK_EXT_mesh_shader","VK_AMD_shader_core_properties","VK_AMD_shader_core_properties2","VK_AMD_shader_info","VK_KHR_pipeline_executable_properties","VK_SEC_amigo_profiling","VK_EXT_descriptor_buffer","VK_KHR_performance_query","VK_EXT_shader_atomic_float","VK_KHR_ray_tracing_pipeline"})o<<name<<": "<<(has(name)?"ANUNCIADA":"NAO ANUNCIADA")<<"\n";
 }
 o<<"\n[Graphics libraries loaded in probe process]\n";{ifstream maps("/proc/self/maps");string line;vector<string> paths;while(getline(maps,line)){auto pos=line.find('/');if(pos==string::npos)continue;auto path=line.substr(pos);if(path.find("vulkan")!=string::npos||path.find("/egl/")!=string::npos||path.find("sgpu")!=string::npos||path.find("xclipse")!=string::npos)paths.push_back(path);}sort(paths.begin(),paths.end());paths.erase(unique(paths.begin(),paths.end()),paths.end());for(auto& p:paths)o<<p<<"\n";}
 return o.str();
}
static string glscan(){ostringstream o;EGLDisplay d=eglGetDisplay(EGL_DEFAULT_DISPLAY);EGLint maj=0,min=0;if(d==EGL_NO_DISPLAY||!eglInitialize(d,&maj,&min))return "EGL indisponivel";auto es=[&](EGLint k){const char* s=eglQueryString(d,k);return s?s:"indisponivel";};o<<"EGL "<<maj<<"."<<min<<"\n"<<es(EGL_VENDOR)<<"\n"<<es(EGL_EXTENSIONS)<<"\n";eglBindAPI(EGL_OPENGL_ES_API);EGLint attrs[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,0x40,EGL_NONE};EGLConfig cfg;EGLint n=0;if(!eglChooseConfig(d,attrs,&cfg,1,&n)||!n){attrs[3]=4;if(!eglChooseConfig(d,attrs,&cfg,1,&n)||!n){eglTerminate(d);return o.str()+"Sem config EGL";}}EGLint surf[]={EGL_WIDTH,1,EGL_HEIGHT,1,EGL_NONE};auto s=eglCreatePbufferSurface(d,cfg,surf);EGLint ca[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};auto c=eglCreateContext(d,cfg,EGL_NO_CONTEXT,ca);if(c==EGL_NO_CONTEXT){ca[1]=2;c=eglCreateContext(d,cfg,EGL_NO_CONTEXT,ca);}if(c!=EGL_NO_CONTEXT&&s!=EGL_NO_SURFACE&&eglMakeCurrent(d,s,s,c)){for(auto k:{GL_VENDOR,GL_RENDERER,GL_VERSION,GL_SHADING_LANGUAGE_VERSION,GL_EXTENSIONS}){auto v=glGetString(k);o<<(v?(const char*)v:"indisponivel")<<"\n";}for(auto k:{GL_MAX_TEXTURE_SIZE,GL_MAX_VERTEX_ATTRIBS,GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS}){GLint v=0;glGetIntegerv(k,&v);o<<"limit 0x"<<hex<<k<<dec<<"="<<v<<"\n";}}else o<<"Falha ao criar contexto GLES\n";eglMakeCurrent(d,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);if(c!=EGL_NO_CONTEXT)eglDestroyContext(d,c);if(s!=EGL_NO_SURFACE)eglDestroySurface(d,s);eglTerminate(d);return o.str();}
static string clscan(){ostringstream o;void* h=dlopen("libOpenCL.so",RTLD_NOW|RTLD_LOCAL);if(!h){auto err=dlerror();return string("OpenCL inacessivel neste namespace: ")+(err?err:"");}using U=uint32_t;using I=int32_t;using P=void*;auto platforms=(I(*)(U,P*,U*))dlsym(h,"clGetPlatformIDs");auto devices=(I(*)(P,uint64_t,U,P*,U*))dlsym(h,"clGetDeviceIDs");auto pi=(I(*)(P,U,size_t,void*,size_t*))dlsym(h,"clGetPlatformInfo");auto di=(I(*)(P,U,size_t,void*,size_t*))dlsym(h,"clGetDeviceInfo");if(platforms&&devices&&pi&&di){auto str=[&](auto fn,P p,U k){size_t len=0;if(fn(p,k,0,nullptr,&len)||len==0||len>1048576)return string("indisponivel");vector<char> b(len+1);if(fn(p,k,len,b.data(),nullptr))return string("indisponivel");return string(b.data());};U n=0;auto r=platforms(0,nullptr,&n);o<<"platform result="<<r<<" count="<<n<<"\n";if(!r&&n<128){vector<P> ps(n);if(!platforms(n,ps.data(),nullptr))for(auto p:ps){o<<str(pi,p,0x0902)<<" "<<str(pi,p,0x0901)<<"\n";U k=0;if(!devices(p,0xffffffffULL,0,nullptr,&k)&&k<128){vector<P> ds(k);if(!devices(p,0xffffffffULL,k,ds.data(),nullptr))for(auto d:ds){for(auto id:{0x102B,0x102C,0x102D,0x102F,0x1030,0x103D})o<<"0x"<<hex<<id<<dec<<"="<<str(di,d,id)<<"\n";U units=0;if(!di(d,0x1002,sizeof(units),&units,nullptr))o<<"compute units="<<units<<"\n";}}}}}else o<<"OpenCL carregada, API incompleta\n";dlclose(h);return o.str();}
std::string compute_scan();
std::string fma_scan();
std::string hp_scan();
std::string opencl_compute();
extern "C" JNIEXPORT jbyteArray JNICALL Java_dev_xclipse_gpulab_Native_scan(JNIEnv* e,jclass,jint mode,jstring name){try{return result(e,mode==1?glscan():mode==2?clscan():mode==3?compute_scan():mode==4?opencl_compute():mode==5?fma_scan():mode==6?hp_scan():vulkan(fromjava(e,name)));}catch(const exception& x){return result(e,x.what());}}
