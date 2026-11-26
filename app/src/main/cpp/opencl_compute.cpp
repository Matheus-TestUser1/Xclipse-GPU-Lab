#include <dlfcn.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <sstream>
#include <vector>
#include <iomanip>
#include <stdexcept>
namespace {
using I=int32_t;using U=uint32_t;using L=uint64_t;using P=void*;using Z=size_t;using IP=intptr_t;
template<class T>T load(P lib,const char* n){T f=reinterpret_cast<T>(dlsym(lib,n));if(!f)throw std::runtime_error(std::string("Missing OpenCL symbol ")+n);return f;}
void chk(I r,const char* step){if(r)throw std::runtime_error(std::string(step)+" CL error="+std::to_string(r));}
struct Lib{P p=dlopen("libOpenCL.so",RTLD_NOW|RTLD_LOCAL);~Lib(){if(p)dlclose(p);}};
struct Objects{P ctx{},queue{},prog{},kernel{},buf{};I(*rc)(P){};I(*rq)(P){};I(*rp)(P){};I(*rk)(P){};I(*rm)(P){};~Objects(){if(buf)rm(buf);if(kernel)rk(kernel);if(prog)rp(prog);if(queue)rq(queue);if(ctx)rc(ctx);}};
}
std::string opencl_compute(){
 std::ostringstream o;o<<"[OpenCL functional tests: own kernels / compiler binaries]\n";
 try{Lib l;if(!l.p)return "INCONCLUSIVE: libOpenCL.so inaccessible";
 auto platforms=load<I(*)(U,P*,U*)>(l.p,"clGetPlatformIDs");
 auto devices=load<I(*)(P,L,U,P*,U*)>(l.p,"clGetDeviceIDs");
 auto info=load<I(*)(P,U,Z,void*,Z*)>(l.p,"clGetDeviceInfo");
 auto createContext=load<P(*)(const IP*,U,const P*,void(*)(const char*,const void*,Z,void*),void*,I*)>(l.p,"clCreateContext");
 auto createQueue=load<P(*)(P,P,L,I*)>(l.p,"clCreateCommandQueue");
 auto createProgram=load<P(*)(P,U,const char**,const Z*,I*)>(l.p,"clCreateProgramWithSource");
 auto build=load<I(*)(P,U,const P*,const char*,void(*)(P,void*),void*)>(l.p,"clBuildProgram");
 auto programInfo=load<I(*)(P,U,Z,void*,Z*)>(l.p,"clGetProgramInfo");
 auto buildInfo=load<I(*)(P,P,U,Z,void*,Z*)>(l.p,"clGetProgramBuildInfo");
 auto createKernel=load<P(*)(P,const char*,I*)>(l.p,"clCreateKernel");
 auto createBuffer=load<P(*)(P,L,Z,void*,I*)>(l.p,"clCreateBuffer");
 auto arg=load<I(*)(P,U,Z,const void*)>(l.p,"clSetKernelArg");
 auto run=load<I(*)(P,P,U,const Z*,const Z*,const Z*,U,const P*,P*)>(l.p,"clEnqueueNDRangeKernel");
 auto read=load<I(*)(P,P,U,Z,Z,void*,U,const P*,P*)>(l.p,"clEnqueueReadBuffer");
 U n=0;chk(platforms(0,nullptr,&n),"platforms");if(!n||n>64)return "INCONCLUSIVE no platform";std::vector<P> ps(n);chk(platforms(n,ps.data(),nullptr),"platform list");
 for(P p:ps){U k=0;if(devices(p,4,0,nullptr,&k)||!k||k>32)continue;std::vector<P> ds(k);chk(devices(p,4,k,ds.data(),nullptr),"GPU devices");
 for(P d:ds){auto str=[&](U key){Z z=0;if(info(d,key,0,nullptr,&z)||!z||z>65536)return std::string();std::vector<char>b(z+1);if(info(d,key,z,b.data(),nullptr))return std::string();return std::string(b.data());};std::string ext=str(0x1030);o<<"\n[GPU] "<<str(0x102b)<<"\nCL_DRIVER="<<str(0x102d)<<"\n";
 auto scalar=[&](const char* name,U key){U v=0;I rc=info(d,key,sizeof(v),&v,nullptr);o<<name<<"=";if(rc)o<<"unavailable ("<<rc<<")";else o<<v;o<<"\n";};
 scalar("CL_COMPUTE_UNITS",0x1002);
 if(ext.find("cl_amd_device_attribute_query")!=std::string::npos){scalar("CL_SIMD_PER_CU_AMD",0x4040);scalar("CL_SIMD_WIDTH_AMD",0x4041);scalar("CL_SIMD_INSTRUCTION_WIDTH_AMD",0x4042);scalar("CL_WAVEFRONT_WIDTH_AMD",0x4043);scalar("CL_GFXIP_MAJOR_AMD",0x404A);scalar("CL_GFXIP_MINOR_AMD",0x404B);}
 for(int test=0;test<4;test++){const char* names[]={"UINT32","INT8 arithmetic","INT16 dot (scalar expression)","FP16"};o<<"\n[CL TEST] "<<names[test]<<"\n";if(test==3&&ext.find("cl_khr_fp16")==std::string::npos){o<<"SKIP extension not advertised\n";continue;}
 try{Objects x;x.rc=load<I(*)(P)>(l.p,"clReleaseContext");x.rq=load<I(*)(P)>(l.p,"clReleaseCommandQueue");x.rp=load<I(*)(P)>(l.p,"clReleaseProgram");x.rk=load<I(*)(P)>(l.p,"clReleaseKernel");x.rm=load<I(*)(P)>(l.p,"clReleaseMemObject");I err=0;
 x.ctx=createContext(nullptr,1,&d,nullptr,nullptr,&err);chk(err,"context");x.queue=createQueue(x.ctx,d,0,&err);chk(err,"queue");
 const char* source[]={ "__kernel void probe(__global uint *b){uint i=get_global_id(0); b[i]=b[64+i]*7u+3u;}", "__kernel void probe(__global uint *b){uint i=get_global_id(0);char a=(char)b[64+i];char r=(char)(a*3+1);b[i]=(uint)r;}", "__kernel void probe(__global uint *b){uint i=get_global_id(0);short a=(short)b[64+i]; short2 x=(short2)(a,a+1),y=(short2)(2,3);b[i]=(uint)((int)x.s0*y.s0+(int)x.s1*y.s1);}", "#pragma OPENCL EXTENSION cl_khr_fp16 : enable\n__kernel void probe(__global uint *b){uint i=get_global_id(0);half a=convert_half(b[64+i]); half r=a*(half)1.5f+(half)0.5f;b[i]=as_uint(convert_float(r));}"};
 const char* src=source[test];x.prog=createProgram(x.ctx,1,&src,nullptr,&err);chk(err,"program");err=build(x.prog,1,&d,"-cl-std=CL1.2",nullptr,nullptr);
 if(err){Z size=0;buildInfo(x.prog,d,0x1183,0,nullptr,&size);if(size&&size<65536){std::vector<char>b(size+1);if(!buildInfo(x.prog,d,0x1183,size,b.data(),nullptr))o<<"Compiler log: "<<b.data()<<"\n";}chk(err,"build");}
 Z bytes=0;if(!programInfo(x.prog,0x1165,sizeof(bytes),&bytes,nullptr)){o<<"CL_BINARY_BYTES="<<bytes<<"\n";if(bytes&&bytes<=262144){std::vector<unsigned char> b(bytes);unsigned char* ptr=b.data();if(!programInfo(x.prog,0x1166,sizeof(ptr),&ptr,nullptr)){o<<"CL_BINARY_HEX_BEGIN\n";for(Z i=0;i<bytes;i++){o<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(b[i]);if((i+1)%32==0)o<<"\n";}o<<std::dec<<std::setfill(' ')<<"\nCL_BINARY_HEX_END\n";}}else o<<"Binary export skipped: size limit 256 KiB\n";}
 x.kernel=createKernel(x.prog,"probe",&err);chk(err,"kernel");uint32_t data[128]{};for(U i=0;i<64;i++)data[64+i]=1+i%8;x.buf=createBuffer(x.ctx,1|32,sizeof(data),data,&err);chk(err,"buffer");chk(arg(x.kernel,0,sizeof(P),&x.buf),"argument");Z global=64;chk(run(x.queue,x.kernel,1,nullptr,&global,nullptr,0,nullptr,nullptr),"enqueue");chk(read(x.queue,x.buf,1,0,sizeof(data),data,0,nullptr,nullptr),"read");
 U mismatch=0;for(U i=0;i<64;i++){U a=1+i%8,expected=a*7+3;if(test==1)expected=a*3+1;if(test==2)expected=a*5+3;if(test==3){float f=a*1.5f+0.5f;std::memcpy(&expected,&f,4);}if(data[i]!=expected)mismatch++;}o<<(mismatch?"FAIL":"PASS")<<" mismatch="<<mismatch<<"/64\n";
 }catch(const std::exception& e){o<<"INCONCLUSIVE "<<e.what()<<"\n";}}
 }}
 }catch(const std::exception& e){o<<"INCONCLUSIVE "<<e.what()<<"\n";}return o.str()+"\nOpenCL compiler binaries are emitted as hex for own test kernels. Scalar dot expression is not proof of a native dot opcode. No external binaries executed.\n";
}
