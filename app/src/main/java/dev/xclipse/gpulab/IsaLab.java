package dev.xclipse.gpulab;
import java.util.*;
import java.util.regex.*;
final class IsaLab {
 static final Pattern AMD=Pattern.compile("^\\s*([a-z][a-z0-9_]+)\\s*.*?//\\s*([0-9A-Fa-f]+):\\s*((?:[0-9A-Fa-f]{8}\\s*)+)$");
 static final Pattern LLVM=Pattern.compile("^\\s*([a-z][a-z0-9_]+)\\s*.*?;\\s*([0-9A-Fa-f]+):\\s*((?:[0-9A-Fa-f]{8}\\s*)+)$");
 static final Pattern ASM=Pattern.compile("^\\s*(s_|v_|ds_|buffer_|global_|flat_|image_|scratch_|tbuffer_|exp_)[a-z0-9_]+\\b.*");
 static final class Entry {final Set<String> words=new TreeSet<>(),tests=new TreeSet<>(),examples=new LinkedHashSet<>();int count;}
 static final class Catalog {final Map<String,Entry> ops=new TreeMap<>();final Set<String> arch=new TreeSet<>(),waves=new TreeSet<>();}
 static Catalog parse(String report){
  Catalog c=new Catalog();String test="unlabelled",wave="?";boolean block=false;
  for(String line:report.split("\n")){
   String t=line.trim();if(t.startsWith("[TEST]")){test=t.substring(6).split("\\|",2)[0].trim();block=false;wave="?";continue;}
   if(t.startsWith("asic(")){c.arch.add(t);block=true;continue;}
   if(t.startsWith("wave_size(")){c.waves.add(t);wave=t;continue;}
   if(t.startsWith("[AMD driver disassembly]")||t.startsWith("; -------- Disassembly")||t.startsWith("IR "))block=true;
   if(t.startsWith("PASS mismatch")||t.startsWith("FAIL mismatch")||t.startsWith("AMD statistics"))block=false;
   Matcher m=AMD.matcher(line);if(!m.matches())m=LLVM.matcher(line);
   if(!m.matches()){if(block&&ASM.matcher(line).matches()){String op=t.split("\\s+",2)[0];if(op.equals("s_code_end"))continue;Entry e=c.ops.computeIfAbsent(op,k->new Entry());e.count++;e.tests.add(test+" / "+wave);if(e.examples.size()<3)e.examples.add(t);}continue;}
   String op=m.group(1);if(!ASM.matcher(t).matches()||op.equals("s_code_end"))continue;Entry e=c.ops.computeIfAbsent(op,k->new Entry());e.count++;if(e.words.size()<128)e.words.add(m.group(3).trim().toUpperCase(Locale.ROOT).replaceAll("\\s+"," "));e.tests.add(test+" / "+wave);if(e.examples.size()<3)e.examples.add(t);
  }return c;
 }
 static String catalog(String report){Catalog c=parse(report);StringBuilder b=new StringBuilder("[ISA evidence / driver-returned text]\nArchitecture tags: "+c.arch+"\nWave tags: "+c.waves+"\nUnique mnemonics: "+c.ops.size()+"\n");
  if(c.ops.isEmpty())return b.append("INCONCLUSIVE: no recognized disassembly. Run compute or import a report.\n").toString();
  for(Map.Entry<String,Entry> x:c.ops.entrySet()){Entry e=x.getValue();b.append("\n").append(x.getKey()).append(" occurrences=").append(e.count).append("\n  Tests: ").append(e.tests).append("\n  Observed words: ").append(e.words).append('\n');for(String s:e.examples)b.append("  ").append(s).append('\n');}
  return b.append("\nRepeated driver IR can duplicate counts. Words include operands; not an opcode mask. Missing mnemonic is not proof of unsupported ISA. No binary execution.\n").toString();}
 static String compare(String baseline,String current){Catalog a=parse(baseline),b=parse(current);StringBuilder s=new StringBuilder("[ISA A vs B]\nA "+a.arch+" "+a.ops.size()+" mnemonics\nB "+b.arch+" "+b.ops.size()+" mnemonics\n");if(a.ops.isEmpty()||b.ops.isEmpty())return s.append("INCONCLUSIVE: both inputs need recognizable disassembly.\n").toString();
  TreeSet<String> onlyA=new TreeSet<>(a.ops.keySet()),onlyB=new TreeSet<>(b.ops.keySet()),both=new TreeSet<>(a.ops.keySet());onlyA.removeAll(b.ops.keySet());onlyB.removeAll(a.ops.keySet());both.retainAll(b.ops.keySet());s.append("Only A: ").append(onlyA).append("\nOnly B: ").append(onlyB).append("\nCommon: ").append(both).append('\n');
  for(String n:both){Entry x=a.ops.get(n),y=b.ops.get(n);if(!x.words.isEmpty()&&!y.words.isEmpty()){Set<String> overlap=new TreeSet<>(x.words);overlap.retainAll(y.words);s.append(n).append(" exact observed word matches=").append(overlap.size()).append(" A=").append(x.words.size()).append(" B=").append(y.words.size()).append('\n');}}
  return s.append("Different shader inputs, register allocation, operands or wave size can change encodings. Compare equivalent shaders and do not treat nonmatching words as incompatible opcodes.\n").toString();}
 static String topology(String vk,String cl,String elf){Map<String,Set<String>> v=Research.fields(vk);StringBuilder s=new StringBuilder("[Cross-API topology / declared values]\n");for(String k:new String[]{"shaderEngineCount","shaderArraysPerEngineCount","computeUnitsPerShaderArray","activeComputeUnitCount","simdPerComputeUnit","wavefrontSize","minSubgroupSize","maxSubgroupSize","maxComputeSharedMemorySize","integerDotProduct8BitSignedAccelerated","integerDotProduct16BitSignedAccelerated","amigoProfiling"})s.append(k).append('=').append(v.getOrDefault(k,Collections.emptySet())).append('\n');
  for(String l:cl.split("\n"))if(l.startsWith("compute units=")||l.startsWith("CL_"))s.append(l).append('\n');s.append("Vulkan CU count and OpenCL compute units are API-specific. Do not assume physical CU/WGP equivalence.\n\n[Private/hidden extension evidence]\n");Set<String> ex=Research.extensions(vk);for(String n:new String[]{"VK_AMD_gpa_interface","VK_SEC_amigo_profiling","VK_EXT_shader_atomic_float","VK_EXT_shader_atomic_float2"})s.append(n).append(": advertised=").append(ex.contains(n)).append(" ELF text hint=").append(elf.contains(n)).append('\n');return s.append("Private extension commands are not invoked; ABI and required permissions are not inferred from strings.\nMesh Shader: user reports RADV execution; not independently verified in this capture.\nCooperative Matrix: user reports RADV ALU/subgroup fallback; native WMMA remains undetermined.\nNot advertised is a property of the active driver, not the silicon.\n").toString();}
}
