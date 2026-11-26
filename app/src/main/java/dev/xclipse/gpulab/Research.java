package dev.xclipse.gpulab;
import java.util.*;
final class Research {
 static final String[][] ITEMS={
 {"Topology / shader cores","VK_AMD_shader_core_properties","shaderEngineCount","computeUnitsPerShaderArray","simdPerComputeUnit","wavefrontSize"},
 {"Packed FP16","VK_KHR_shader_float16_int8","shaderFloat16"},
 {"INT8","VK_KHR_shader_float16_int8","shaderInt8"},
 {"INT16","CORE","shaderInt16"},
 {"INT64","CORE","shaderInt64"},
 {"FP64","CORE","shaderFloat64"},
 {"Integer dot product","VK_KHR_shader_integer_dot_product","shaderIntegerDotProduct","integerDotProduct8BitSignedAccelerated"},
 {"Wave size selection","VK_EXT_subgroup_size_control","subgroupSizeControl","minSubgroupSize","maxSubgroupSize","requiredSubgroupSizeStages"},
 {"Cooperative matrix / WMMA candidate","VK_KHR_cooperative_matrix","cooperativeMatrix"},
 {"Atomic float","VK_EXT_shader_atomic_float","shaderBufferFloat32Atomics","shaderBufferFloat32AtomicAdd"},
 {"Atomic float2","VK_EXT_shader_atomic_float2","shaderBufferFloat16Atomics","shaderBufferFloat16AtomicAdd"},
 {"Mesh shader (RADV execution reported by user)","VK_EXT_mesh_shader","meshShader","taskShader"},
 {"Ray query","VK_KHR_ray_query","rayQuery"},
 {"Shader executable metadata","VK_KHR_pipeline_executable_properties","pipelineExecutableInfo"},
 {"AMD shader information","VK_AMD_shader_info"},
 {"Samsung profiling (private)","VK_SEC_amigo_profiling"},
 {"Performance counter queries","VK_KHR_performance_query","performanceCounterQueryPools"},
 {"Cache / LDS (shared memory limit)","CORE","maxComputeSharedMemorySize"}
 };
 static Set<String> extensions(String report){Set<String> s=new TreeSet<>();boolean active=false;for(String line:report.split("\n")){if(line.startsWith("[Device extensions:")){active=true;continue;}if(line.startsWith("["))active=false;if(active&&line.matches("VK_[A-Za-z0-9_]+ r[0-9]+"))s.add(line.split(" ")[0]);}return s;}
 static Map<String,Set<String>> fields(String r){Map<String,Set<String>> m=new TreeMap<>();for(String line:r.split("\n")){int n=line.indexOf('=');if(n<=0)continue;String k=line.substring(0,n),v=line.substring(n+1);if(k.matches("[A-Za-z][A-Za-z0-9_]*")&&v.matches("[0-9]+"))m.computeIfAbsent(k,x->new TreeSet<>()).add(v);}return m;}
 static String profile(String vk,String elf,String tests){
  StringBuilder b=new StringBuilder("[Silicon Explorer / RDNA 3 reference, not physical layout]\nNo software die photo. A feature bit is not silicon proof. Multiple GPU reports can mix values; compare single GPU captures.\n");
  if(vk.isEmpty())b.append("Run Vulkan profundo first.\n");
  Set<String> ex=extensions(vk);Map<String,Set<String>> fs=fields(vk);
  for(String[] a:ITEMS){boolean core=a[1].equals("CORE"),adv=ex.contains(a[1]);boolean hint=!core&&!adv&&elf.contains(a[1]);int yes=0,no=0;boolean conflict=false;
   for(int i=2;i<a.length;i++){Set<String> v=fs.get(a[i]);if(v==null)continue;if(v.size()>1)conflict=true;if(a[i].startsWith("shader")&&!a[i].equals("shaderEngineCount")||a[i].equals("integerDotProduct8BitSignedAccelerated")||a[i].equals("subgroupSizeControl")||a[i].equals("cooperativeMatrix")||a[i].equals("meshShader")||a[i].equals("taskShader")||a[i].equals("rayQuery")||a[i].equals("pipelineExecutableInfo")||a[i].equals("performanceCounterQueryPools")){if(v.contains("1"))yes++;if(v.contains("0"))no++;}}
   b.append("\n").append(a[0]).append(" | ").append(vk.isEmpty()?"INCONCLUSIVE":conflict?"CONFLICTING VALUES":yes>0&&no>0?"PARTIAL (optional bits)":yes>0?"FEATURE BIT TRUE":no>0?"FEATURE BIT FALSE":adv?"ADVERTISED / NOT FUNCTIONALLY TESTED":core?"LIMIT / INCONCLUSIVE":hint?"ELF HINT ONLY":"NOT ADVERTISED BY ACTIVE DRIVER (silicon undetermined)").append('\n');
   if(!core)b.append("  ").append(a[1]).append('\n');for(int i=2;i<a.length;i++)if(fs.containsKey(a[i]))b.append("  ").append(a[i]).append('=').append(fs.get(a[i])).append('\n');
  }
  b.append("\n[Actual compute test report, if run]\n").append(tests.isEmpty()?"Not run.\n":tests);
  return b.append("\nISA candidates: v_pk_fma_f16, v_dot4_i32_i8, v_wmma_f32_16x16x16_f16, wave32/wave64, LDS. Shader tests validate API operations; disassembly is separate evidence of emitted instructions. User reports RADV Mesh Shader and cooperative-matrix fallback via ALU/subgroups. Native WMMA/MFMA remains undetermined.\n").toString();
 }
 static String group(int group,String vk,String elf,String tests){
  if(group==6)return profile(vk,elf,tests);
  if(group==7)return "[RDNA 3 instruction candidates / not a disassembly]\n"+
   "v_pk_fma_f16: packed FP16 candidate; FP16 shader PASS does not prove opcode.\n"+
   "v_dot4_i32_i8: packed integer dot candidate; query integer dot properties.\n"+
   "v_wmma_f32_16x16x16_f16: WMMA candidate; needs a dedicated native or cooperative matrix test.\n"+
   "ds_read / ds_write: LDS operations; shared memory limit is not ISA validation.\n"+
   "Wave32 / Wave64: pipeline subgroup request and functional subgroup tests.\n"+
   "Reference: https://gpuopen.com/amd-gpu-architecture-programming-documentation/\n\n"+tests;
  String[][] keys={{"Shader executable","AMD shader information","Samsung profiling","Performance counter"},{"Topology"},{"Packed FP16","INT8","INT16","INT64","FP64","Integer dot","Atomic float"},{"Wave size","Cooperative matrix"},{"Cache / LDS"},{"Mesh shader","Ray query"}};
  String full=profile(vk,elf,tests);StringBuilder b=new StringBuilder("[Logical block; no physical coordinates]\n");for(String part:full.split("\\n\\n"))for(String key:keys[group])if(part.startsWith(key)){b.append(part).append("\n\n");break;}return b.append("[Compute evidence]\n").append(tests.isEmpty()?"Not run":tests).toString();
 }
 static String compare(String a,String b){
  Set<String> x=extensions(a),y=extensions(b);StringBuilder s=new StringBuilder("[Report comparison: A saved vs B current]\n");
  if(x.isEmpty()||y.isEmpty())return s.append("INCONCLUSIVE: both reports must contain device extension lists.\n").toString();
  TreeSet<String> onlyA=new TreeSet<>(x),onlyB=new TreeSet<>(y);onlyA.removeAll(y);onlyB.removeAll(x);
  s.append("A extensions=").append(x.size()).append(" B extensions=").append(y.size()).append("\nOnly A:\n");for(String n:onlyA)s.append(n).append('\n');s.append("Only B:\n");for(String n:onlyB)s.append(n).append('\n');
  Map<String,Set<String>> af=fields(a),bf=fields(b);for(String n:af.keySet())if(bf.containsKey(n)&&!af.get(n).equals(bf.get(n)))s.append("DIFF ").append(n).append(": ").append(af.get(n)).append(" -> ").append(bf.get(n)).append('\n');
  return s.append("Compare reports for same physical GPU. This does not load alternative drivers or prove functionality.\n").toString();
 }
}
