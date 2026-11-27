package dev.xclipse.gpulab;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.*;
final class Kernel {
 // A fixed, read-only script; no user-controlled shell arguments or writes.
 static final String TELEMETRY="for f in gpu_model gpu_clock gpu_busy gpu_tmu; do echo \"/sys/kernel/gpu/$f:\"; head -c 4096 \"/sys/kernel/gpu/$f\" 2>&1; echo; done; for d in /sys/class/devfreq/* /sys/class/kgsl/kgsl-3d0 /sys/class/misc/mali0/device /sys/devices/platform/*sgpu* /sys/devices/platform/*gpu* /sys/devices/platform/*mali*; do " +
 "[ -d \"$d\" ] || continue; case \"$d\" in *devfreq*) name=$(cat \"$d/name\" 2>/dev/null); case \"$d $name\" in *gpu*|*GPU*|*mali*|*Mali*|*sgpu*|*g3d*|*G3D*|*kgsl*|*xclipse*) ;; *) continue;; esac;; esac; " +
 "echo \"[$d]\"; for f in name cur_freq min_freq max_freq available_frequencies governor load utilization gpu_busy_percent gpubusy gpuclk max_gpuclk min_gpuclk gpu_freq gpu_clock gpu_busy gpu_utilization gpu_usage gpu_temp temperature current_utilization current_cu_utilization utilization_source compute_weight time_in_state trans_stat; do if [ -e \"$d/$f\" ]; then echo \"$f:\"; head -c 4096 \"$d/$f\" 2>&1; echo; fi; done; done; " +
 "for d in /sys/class/thermal/thermal_zone*; do [ -r \"$d/type\" ] || continue; name=$(cat \"$d/type\" 2>/dev/null); case \"$name\" in *gpu*|*GPU*|*G3D*|*g3d*|*sgpu*|*mali*) echo \"thermal $name: $(cat \"$d/temp\" 2>&1)\";; esac; done";
 static final String DEEP="echo '[GPU properties]'; getprop | grep -iE 'vulkan|gpu|egl|gles|xclipse|sgpu|gralloc|angle|opencl|mali'; echo '[DRM devices]'; ls -l /dev/dri /dev/kgsl-3d0 /dev/mali0 2>&1; echo '[GPU kernel paths]'; ls -d /sys/class/drm/* /sys/kernel/debug/dri/* /sys/module/*gpu* /sys/module/*mali* /sys/module/*drm* 2>&1; echo '[DRM read-only diagnostics]'; for d in /sys/kernel/debug/dri/*; do [ -d \"$d\" ] || continue; for f in name amdgpu_pm_info amdgpu_firmware_info amdgpu_gpu_info amdgpu_vram_mm amdgpu_gtt_mm; do if [ -e \"$d/$f\" ]; then echo \"$d/$f\"; head -c 8192 \"$d/$f\" 2>&1; echo; fi; done; done; echo '[GPU trace sources: availability only]'; for d in /sys/kernel/tracing/events /sys/kernel/debug/tracing/events; do ls -d \"$d\"/*gpu* \"$d\"/*drm* \"$d\"/*sgpu* \"$d\"/*mali* 2>&1; done; echo '[GPU memory accounting]'; ls -d /sys/kernel/dmabuf/buffers /sys/class/drm/*/device /sys/bus/event_source/devices/*gpu* 2>&1; echo '[dumpsys gpu]'; dumpsys gpu 2>&1; echo '[GPU driver libraries]'; ls -l /vendor/lib64/hw/*vulkan* /vendor/lib64/egl/* /vendor/lib64/*OpenCL* /odm/lib64/hw/*vulkan* 2>&1; "+TELEMETRY;
 static String run(boolean root,boolean deep){return runScript(root,deep?DEEP:TELEMETRY,deep?15:5);}
 static String runScript(boolean root,String script,int timeoutSeconds){
  Process p=null; ExecutorService ex=Executors.newSingleThreadExecutor(); Capture capture=new Capture();
  try {
   p=new ProcessBuilder(root?new String[]{"su","-c",script}:new String[]{"/system/bin/sh","-c",script}).redirectErrorStream(true).start();
   final Process proc=p;
   Future<?> reader=ex.submit(()->{try(InputStream in=proc.getInputStream()){byte[] bytes=new byte[4096];int n;while((n=in.read(bytes))!=-1)capture.append(bytes,n);}catch(IOException ignored){}});
   boolean done=p.waitFor(timeoutSeconds,TimeUnit.SECONDS);
   if(!done)p.destroyForcibly();
   try{reader.get(2,TimeUnit.SECONDS);}catch(TimeoutException e){reader.cancel(true);}
   return "Modo="+(root?"root solicitado":"app normal")+" "+(done?"exit="+p.exitValue():"TIMEOUT: captura parcial")+"\n"+capture.text();
  }catch(InterruptedException e){Thread.currentThread().interrupt();return "Consulta interrompida; captura parcial\n"+capture.text();}
  catch(Exception e){return "Consulta indisponivel: "+e+"\n"+capture.text();}
  finally{if(p!=null){p.destroyForcibly();try{p.getInputStream().close();}catch(IOException ignored){}}ex.shutdownNow();}
 }
 static final class Capture {
  static final int LIMIT=512*1024;
  private final ByteArrayOutputStream bytes=new ByteArrayOutputStream(); private boolean truncated;
  synchronized void append(byte[] data,int n){int keep=Math.min(n,LIMIT-bytes.size());if(keep>0)bytes.write(data,0,keep);if(keep<n)truncated=true;}
  synchronized String text(){String text=new String(bytes.toByteArray(),StandardCharsets.UTF_8);return text+(truncated?"\n[OUTPUT TRUNCATED: 512 KiB]\n":"");}
 }
}
