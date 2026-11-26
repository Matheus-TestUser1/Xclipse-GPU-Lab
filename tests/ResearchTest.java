package dev.xclipse.gpulab;
import java.io.File;
final class ResearchTest {
 public static void main(String[] args)throws Exception{
  String a="[GPU] Xclipse\n[Device extensions: anunciadas]\nVK_KHR_shader_float16_int8 r1\nVK_EXT_mesh_shader r1\n[Core features]\nshaderFloat16=1\nshaderInt8=0\n";
  String b=a.replace("VK_EXT_mesh_shader r1\n","").replace("shaderFloat16=1","shaderFloat16=0");
  String p=Research.profile(a,"","");
  if(!p.contains("FEATURE BIT TRUE")||!p.contains("FEATURE BIT FALSE"))throw new AssertionError(p);
  String diff=Research.compare(a,b);
  if(!diff.contains("VK_EXT_mesh_shader")||!diff.contains("DIFF shaderFloat16"))throw new AssertionError(diff);
  Elf.Info elf=Elf.read(new File("/bin/ls"));if(elf.bits!=64||!elf.sections||elf.needed.isEmpty()||elf.imports.isEmpty())throw new AssertionError(elf.render());
  System.out.println("PASS Research + ELF");
 }
}
