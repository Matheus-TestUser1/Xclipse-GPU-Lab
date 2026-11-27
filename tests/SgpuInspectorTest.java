package dev.xclipse.gpulab;

import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;

class SgpuInspectorTest {
 static void check(boolean condition,String message){if(!condition)throw new AssertionError(message);}
 public static void main(String[] args)throws Exception {
  String raw="Modo=app normal exit=0\n@@SGPU|/sys/kernel/gpu/gpu_model\nSamsung Xclipse 940\n\n@@END|0\n"+
   "@@SGPU|/sys/kernel/gpu/gpu_busy\nhead: Permission denied\n@@END|1\n"+
   "@@SGPU|/empty\n\n@@END|0\n@@SGPU|/table\nfreq voltage\n500 800\n@@END|0\n@@SGPU|/partial\n42";
  List<SgpuInspector.Reading> r=SgpuInspector.parse(raw);
  check(r.size()==5,"all readings");
  check(r.get(0).state.equals("READ")&&r.get(0).path.equals("/sys/kernel/gpu/gpu_model"),"model and path");
  check(r.get(1).state.equals("UNAVAILABLE")&&r.get(1).value.contains("Permission denied"),"retain failed read");
  check(r.get(2).state.equals("EMPTY")&&r.get(3).value.equals("freq voltage\n500 800"),"empty and multiline");
  check(r.get(4).state.equals("INCOMPLETE"),"timeout retains partial node");
  check(SgpuInspector.parse("@@SGPU|/large\n"+"é".repeat(4097)+"\n@@END|0").get(0).state.equals("TRUNCATED"),"byte limit with UTF-8");
  String vk="[GPU] A\nactiveComputeUnitCount=6\n[GPU] B\nactiveComputeUnitCount=8\n";
  String report=SgpuInspector.summarize(raw,vk,"CL_COMPUTE_UNITS=6\nCL_WAVEFRONT_WIDTH_AMD=64\n");
  check(report.contains("READ=2 UNAVAILABLE=1 outras=2"),"counts");
  check(report.contains("Vulkan | activeComputeUnitCount=6")&&report.contains("Vulkan | activeComputeUnitCount=8")&&report.contains("não são somados"),"keep independent devices");
  check(report.endsWith(raw),"raw export preserved");
  check(SgpuInspector.summarize("Consulta indisponivel: su ausente","","").contains("nenhuma leitura estruturada"),"root failure is inconclusive");

  // Execute the real collector against an artificial sysfs tree: validate path discovery,
  // canonical symlink de-duplication, non-GPU exclusion, and per-node missing-file errors.
  Path root=Files.createTempDirectory("inspector-test");
  try {
   Path sys=root.resolve("sys"),gpu=sys.resolve("class/devfreq/sgpu0"),cpu=sys.resolve("class/devfreq/cpu0");
   Files.createDirectories(gpu);Files.createDirectories(cpu);Files.createDirectories(sys.resolve("kernel/gpu"));
   Files.writeString(sys.resolve("kernel/gpu/gpu_model"),"Samsung Xclipse 940\n");
   Files.writeString(gpu.resolve("name"),"sgpu\n");Files.writeString(gpu.resolve("current_cu_utilization"),"73\n");
   Files.writeString(cpu.resolve("name"),"cpu\n");
   Path alias=sys.resolve("class/drm/card0/device/devfreq");Files.createDirectories(alias);Files.createSymbolicLink(alias.resolve("sgpu0"),gpu);
   Process process=new ProcessBuilder("sh","-c",SgpuInspector.SCRIPT.replace("/sys/",sys.toString()+"/")).redirectErrorStream(true).start();
   String output=new String(process.getInputStream().readAllBytes(),StandardCharsets.UTF_8);
   check(process.waitFor()==0,"shell collector completes");
   List<SgpuInspector.Reading> nodes=SgpuInspector.parse(output);
   check(nodes.stream().anyMatch(n->n.path.endsWith("/current_cu_utilization")&&n.value.equals("73")&&n.state.equals("READ")),"discover SGPU counter");
   check(nodes.stream().filter(n->n.path.endsWith("/current_cu_utilization")).count()==1,"deduplicate aliases");
   check(nodes.stream().noneMatch(n->n.path.contains("cpu0")),"exclude CPU devfreq");
   check(nodes.stream().anyMatch(n->n.path.endsWith("/gpu_tmu")&&n.state.equals("UNAVAILABLE")),"missing path stays unavailable");
  }finally{try(var paths=Files.walk(root)){for(Path p:paths.sorted(Comparator.reverseOrder()).toList())Files.delete(p);}}
  System.out.println("PASS SGPU collector, provenance, errors and partial captures");
 }
}
