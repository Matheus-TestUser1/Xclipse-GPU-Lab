package dev.xclipse.gpulab;
import java.io.*;
import java.util.*;
import java.util.regex.*;
import java.security.MessageDigest;
final class DeepLibraries {
 static final String[] ROOTS={"/vendor/lib","/vendor/lib64","/odm/lib","/odm/lib64","/system/lib","/system/lib64","/system_ext/lib","/system_ext/lib64","/product/lib","/product/lib64","/apex"};
 static final Pattern GPU=Pattern.compile("vulkan|gles|(^|[^a-z])egl|opencl|xclipse|sgpu|mali|drm|gralloc|gpu|shader|llvm|spirv|amd|angle|mesa",Pattern.CASE_INSENSITIVE);
 static final Pattern CLUE=Pattern.compile("wmma|mfma|cooperative.?matrix|wave32|wave64|gfx11|mesh.?shader|amigo|shader.?core|subgroup|dot.?product|atomic.?float|fp16|int8",Pattern.CASE_INSENSITIVE);
 static void walk(File d,Map<String,File> all,Set<String> seen,StringBuilder log,int depth)throws IOException{
  if(depth>16||all.size()>=20000||seen.size()>=20000)return;
  String canonical=d.getCanonicalPath();if(!seen.add(canonical))return;File[] fs=d.listFiles();if(fs==null){if(log.length()<20000)log.append("INACCESSIBLE ").append(d).append('\n');return;}
  Arrays.sort(fs,Comparator.comparing(File::getName));for(File f:fs){if(all.size()>=20000)break;if(f.isDirectory()){walk(f,all,seen,log,depth+1);continue;}if(f.getName().matches(".*\\.so(\\.[0-9.]+)?")&&f.isFile()){String c=f.getCanonicalPath();all.putIfAbsent(c,f);}}
 }
 static String scan(String vulkan){
  StringBuilder b=new StringBuilder("[Recursive system ELF inventory: normal app permissions]\n");Map<String,File> all=new TreeMap<>();Set<String> seen=new HashSet<>();for(String root:ROOTS)try{walk(new File(root),all,seen,b,0);}catch(IOException|SecurityException e){b.append(root).append(": ").append(e).append('\n');}
  b.append("Inventoried=").append(all.size()).append(" (max 20000; not every file can be read)\n");
  Map<String,List<File>> byName=new HashMap<>();for(File f:all.values())byName.computeIfAbsent(f.getName(),k->new ArrayList<>()).add(f);
  Deque<File> queue=new ArrayDeque<>();for(File f:all.values())if(GPU.matcher(f.getName()).find())queue.add(f);
  // Seed actual libraries from Vulkan report (no shell or arbitrary external paths).
  for(String line:vulkan.split("\n"))if(all.containsKey(line))queue.addFirst(all.get(line));
  Set<String> done=new HashSet<>();int scanned=0;
  while(!queue.isEmpty()&&scanned<350&&b.length()<3500000){File f=queue.removeFirst();try{String path=f.getCanonicalPath();if(!done.add(path))continue;scanned++;Elf.Info info=Elf.read(f);b.append("\n[ELF] ").append(path).append('\n').append("bits=").append(info.bits).append(" machine=").append(info.machine).append(" sectionTable=").append(info.sections).append(" SONAME=").append(info.soname).append('\n');
   for(String n:info.needed){b.append("NEEDED ").append(n).append('\n');for(File dep:byName.getOrDefault(n,Collections.emptyList()))if(!done.contains(dep.getCanonicalPath()))queue.addLast(dep);}
   b.append("exports=").append(info.exports.size()).append(" imports=").append(info.imports.size()).append(" symbols=").append(info.symbols.size()).append(" omitted=").append(info.omitted).append('\n');
   for(String n:info.exports)if(GPU.matcher(n).find()||CLUE.matcher(n).find()||n.startsWith("vk")||n.startsWith("cl"))b.append("EXPORT ").append(n).append('\n');
   for(String n:info.imports)if(GPU.matcher(n).find()||CLUE.matcher(n).find())b.append("IMPORT ").append(n).append('\n');
   for(String n:info.symbols)if(CLUE.matcher(n).find())b.append("SYMBOL ").append(n).append('\n');
  }catch(Exception e){b.append("UNREADABLE ").append(f).append(": ").append(e).append('\n');}}
  b.append("\n[Full accessible library path inventory]\n");int listed=0;for(String name:all.keySet()){if(b.length()>4000000)break;b.append(name).append('\n');listed++;}b.append("Listed=").append(listed).append("/").append(all.size()).append('\n');b.append("\nParsed=").append(scanned).append(" / remaining candidates=").append(queue.size()).append("\nDependency matches by basename are candidates, not linker resolution. Stripped/sectionless files may expose fewer or no symbols. No private code executed.\n");return b.toString();
 }
 static String firmware(){
  StringBuilder b=new StringBuilder("[Firmware metadata: read-only, no loading]\n");Set<String> seen=new HashSet<>();int[] count={0};for(String root:new String[]{"/vendor/firmware","/vendor/firmware_mnt","/vendor/etc/firmware","/odm/firmware","/odm/etc/firmware","/lib/firmware","/system/etc/firmware"})try{fw(new File(root),b,seen,count,0);}catch(Exception e){b.append(root).append(" ").append(e).append('\n');}return b.append("Candidates=").append(count[0]).append("\nName and SHA-256 identify files, not firmware ISA or active firmware. No disassembly or writes.\n").toString();}
 static void fw(File dir,StringBuilder b,Set<String> seen,int[] count,int depth)throws Exception{if(depth>8||count[0]>=150||!seen.add(dir.getCanonicalPath()))return;File[] fs=dir.listFiles();if(fs==null){if(depth==0)b.append("Unavailable ").append(dir).append('\n');return;}for(File f:fs){if(count[0]>=150)break;if(f.isDirectory()){fw(f,b,seen,count,depth+1);continue;}if(!GPU.matcher(f.getName()).find()&&!f.getName().toLowerCase(Locale.ROOT).matches(".*(gc_|rlc|sos|smu|sdma|mes_|mec|pfp|gfx|cp_|g3d).*"))continue;count[0]++;b.append(f).append(" bytes=").append(f.length());try(RandomAccessFile raf=new RandomAccessFile(f,"r")){byte[] h=new byte[(int)Math.min(64,raf.length())];raf.readFully(h);b.append(" header=");for(byte x:h)b.append(String.format(Locale.ROOT,"%02x",x&255));if(h.length>=4&&h[0]==127&&h[1]==69&&h[2]==76&&h[3]==70){try{Elf.Info info=Elf.read(f);b.append(" ELF bits=").append(info.bits).append(" machine=").append(info.machine).append(" sections=").append(info.sections);}catch(Exception e){b.append(" ELF parse unavailable");}}else b.append(" format=unidentified");}catch(Exception e){b.append(" header unavailable");}if(f.length()>64L*1024*1024){b.append(" skipped hash (size limit)\n");continue;}try(InputStream in=new FileInputStream(f)){MessageDigest md=MessageDigest.getInstance("SHA-256");byte[] a=new byte[65536];int n;long total=0;while((n=in.read(a))!=-1){total+=n;if(total>64L*1024*1024)throw new IOException("size limit");md.update(a,0,n);}b.append(" sha256=");for(byte x:md.digest())b.append(String.format(Locale.ROOT,"%02x",x&255));}catch(Exception e){b.append(" unreadable ").append(e);}b.append('\n');}}
}
