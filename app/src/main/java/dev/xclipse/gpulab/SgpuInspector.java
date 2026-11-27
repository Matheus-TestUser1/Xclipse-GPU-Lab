package dev.xclipse.gpulab;

import java.util.*;

/** Read-only, allowlisted SKI/devfreq inspection; no DRM ioctls or MMIO. */
final class SgpuInspector {
 static final String SOURCE = "Elchanz3/android_kernel_samsung_r12s@3367aefe4a06fd7dff66d79010e60681637b8336";
 static final String SKI = "gpu_model gpu_clock gpu_busy gpu_freq_table gpu_available_frequencies gpu_dvfs_table gpu_governor gpu_available_governor gpu_tmu";
 static final String DEVFREQ = "name cur_freq min_freq max_freq available_frequencies governor available_governors current_utilization current_cu_utilization available_utilization_sources utilization_source compute_weight time_in_state total_kernel_pages";
 static final String DRM = "uevent vendor device revision mem_info_gtt_total mem_info_gtt_used mem_info_vram_total mem_info_vram_used mem_info_vis_vram_total mem_info_vis_vram_used";

 // The payload is framed on separate lines so multiline tables and errors survive export.
 // A failed read says UNAVAILABLE, never that the hardware or node is absent.
 static final String SCRIPT =
  "read_node() { printf '@@SGPU|%s\\n' \"$1\"; head -c 8193 \"$1\" 2>&1; rc=$?; printf '\\n@@END|%s\\n' \"$rc\"; }; " +
  "for f in " + SKI + "; do read_node \"/sys/kernel/gpu/$f\"; done; " +
  "seen='|'; count=0; for d in /sys/class/devfreq/* /sys/class/drm/card*/device/devfreq/* /sys/class/drm/renderD*/device/devfreq/*; do " +
  "[ -d \"$d\" ] || continue; name=$(cat \"$d/name\" 2>/dev/null); gov=$(cat \"$d/governor\" 2>/dev/null); " +
  "case \"$d $name $gov\" in *sgpu*|*gpu*|*GPU*|*g3d*|*G3D*|*mali*|*Mali*|*kgsl*) ;; *) continue;; esac; " +
  "canonical=$(cd \"$d\" 2>/dev/null && pwd -P); [ -n \"$canonical\" ] || canonical=$d; " +
  "case \"$seen\" in *\"|$canonical|\"*) continue;; esac; seen=\"$seen$canonical|\"; count=$((count+1)); [ \"$count\" -le 16 ] || break; " +
  "for f in " + DEVFREQ + "; do read_node \"$d/$f\"; done; done; " +
  "count=0; for d in /sys/class/drm/card*/device /sys/class/drm/renderD*/device; do " +
  "[ -d \"$d\" ] || continue; canonical=$(cd \"$d\" 2>/dev/null && pwd -P); [ -n \"$canonical\" ] || canonical=$d; " +
  "case \"$seen\" in *\"|$canonical|\"*) continue;; esac; seen=\"$seen$canonical|\"; count=$((count+1)); [ \"$count\" -le 16 ] || break; " +
  "for f in " + DRM + "; do read_node \"$d/$f\"; done; done; " +
  "for d in /sys/class/thermal/thermal_zone*; do [ -d \"$d\" ] || continue; name=$(cat \"$d/type\" 2>/dev/null); " +
  "case \"$name\" in *gpu*|*GPU*|*g3d*|*G3D*|*sgpu*) read_node \"$d/type\"; read_node \"$d/temp\";; esac; done";

 static final class Reading {
  final String path, value, state;
  Reading(String path, String value, String state) { this.path=path; this.value=value; this.state=state; }
 }
 static List<Reading> parse(String raw) {
  List<Reading> result=new ArrayList<>(); String path=null; StringBuilder value=new StringBuilder();
  for(String line:raw.split("\\n",-1)) {
   if(line.startsWith("@@SGPU|")) {
    if(path!=null) result.add(new Reading(path,value.toString().trim(),"INCOMPLETE"));
    path=line.substring(7); value.setLength(0);
   } else if(path!=null && line.matches("@@END\\|[0-9]+")) {
    String text=value.toString().trim();
    String state=!line.equals("@@END|0")?"UNAVAILABLE":value.toString().getBytes(java.nio.charset.StandardCharsets.UTF_8).length>8193?"TRUNCATED":text.isEmpty()?"EMPTY":"READ";
    result.add(new Reading(path,text,state)); path=null;
   } else if(path!=null) value.append(line).append('\n');
  }
  if(path!=null) result.add(new Reading(path,value.toString().trim(),"INCOMPLETE"));
  return result;
 }
 static String run(boolean root,String vk,String cl) {
  return summarize(Kernel.runScript(root,SCRIPT,15),vk,cl);
 }
 static String summarize(String raw,String vk,String cl) {
  List<Reading> readings=parse(raw); int ok=0,unavailable=0,incomplete=0;
  StringBuilder b=new StringBuilder("[SGPU Native Inspector]\nFonte de referência: ").append(SOURCE)
   .append("\nA ROM instalada pode usar outra revisão. Leituras sequenciais, não simultâneas.\n");
  for(Reading r:readings) { if(r.state.equals("READ"))ok++; else if(r.state.equals("UNAVAILABLE"))unavailable++; else incomplete++; }
  b.append("READ=").append(ok).append(" UNAVAILABLE=").append(unavailable).append(" outras=").append(incomplete).append('\n');
  if(readings.isEmpty())b.append("INCONCLUSIVO: nenhuma leitura estruturada recebida.\n");
  b.append("\n[Origem e significado dos contadores]\n")
   .append("/sys/kernel/gpu/gpu_busy: busy_time/total_time do devfreq em %, no kernel de referência.\n")
   .append("current_utilization: carga usada pelo governor, ponderada por compute_weight e limitada a 100; não é contagem de CUs.\n")
   .append("current_cu_utilization: cu_busy_time/total_time em %, no kernel de referência; não informa quantos CUs existem.\n")
   .append("time_in_state: residência por frequência, tempo em ms no SGPU de referência.\n")
   .append("gpu_dvfs_table: tabela de frequência/tensão/limiares, não potência medida.\n")
   .append("total_kernel_pages: páginas alocadas pelo driver; sem conversão para bytes sem tamanho de página confirmado.\n")
   .append("Frequências e gpu_tmu preservados como texto bruto: unidades/formatação dependem da ROM.\n")
   .append("\n[Kernel × Vulkan × OpenCL: declarações independentes]\n");
  for(Reading r:readings)if(r.path.endsWith("/gpu_model")&&r.state.equals("READ"))b.append("Kernel gpu_model: ").append(r.value).append('\n');
  appendLines(b,"Vulkan",vk,"[GPU] ","VendorID=","driverID=","activeComputeUnitCount=","wavefrontSize=","subgroupSize=","minSubgroupSize=","maxSubgroupSize=");
  appendLines(b,"OpenCL",cl,"[GPU] ","0x102b=","CL_DRIVER=","CL_COMPUTE_UNITS=","compute units=","CL_WAVEFRONT_WIDTH_AMD=","CL_GFXIP_MAJOR_AMD=","CL_GFXIP_MINOR_AMD=");
  b.append("A associação entre dispositivos das APIs não foi validada. Valores de várias GPUs não são somados ou equiparados.\n")
   .append("Frequência/carga/sensores não comprovam WMMA nem desbloqueio de NPU.\n")
   .append("\n[Leituras por caminho]\n");
  for(Reading r:readings)b.append(r.state).append(" | ").append(r.path).append('\n').append(r.value).append("\n\n");
  return b.append("UNAVAILABLE: falha de leitura; consulte o erro bruto. Pode ser permissão, caminho não visível ou indisponibilidade do driver.\n")
   .append("EMPTY: leitura vazia. INCOMPLETE/TRUNCATED: captura parcial. Não significam ausência física.\n")
   .append("\n[Captura bruta e estado do processo]\n").append(raw).toString();
 }
 static void appendLines(StringBuilder b,String label,String report,String... prefixes) {
  Set<String> lines=new LinkedHashSet<>();
  for(String line:report.split("\\n"))for(String prefix:prefixes)if(line.startsWith(prefix))lines.add(line);
  if(lines.isEmpty())b.append(label).append(": sem dados; execute a consulta correspondente.\n");
  else for(String line:lines)b.append(label).append(" | ").append(line).append('\n');
 }
}
