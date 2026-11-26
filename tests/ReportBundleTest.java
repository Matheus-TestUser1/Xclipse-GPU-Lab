package dev.xclipse.gpulab;
import java.io.*;
import java.util.*;
import java.util.zip.*;
import java.nio.charset.StandardCharsets;
final class ReportBundleTest {
 public static void main(String[] args)throws Exception{
  File f=File.createTempFile("gpulab",".txt");try{try(FileOutputStream o=new FileOutputStream(f)){o.write("Vulkan íntegro\n".getBytes(StandardCharsets.UTF_8));}
  List<ReportBundle.Entry> e=Arrays.asList(new ReportBundle.Entry("Vulkan.txt",f),new ReportBundle.Entry("Kernel","SGPU\n"));
  ByteArrayOutputStream t=new ByteArrayOutputStream();ReportBundle.write(t,e,false);String s=t.toString("UTF-8");if(!s.contains("Vulkan íntegro")||!s.contains("SGPU")||!s.contains("Reports: 2"))throw new AssertionError(s);
  t.reset();ReportBundle.write(t,e,true);try(ZipInputStream z=new ZipInputStream(new ByteArrayInputStream(t.toByteArray()))){Set<String> names=new HashSet<>();ZipEntry n;while((n=z.getNextEntry())!=null){names.add(n.getName());String body=new String(z.readAllBytes(),StandardCharsets.UTF_8);if(n.getName().contains("Vulkan")&&!body.equals("Vulkan íntegro\n"))throw new AssertionError(body);}if(!names.contains("index.txt")||names.size()!=3)throw new AssertionError(names);}
  System.out.println("PASS consolidated TXT and ZIP");}finally{f.delete();}
 }
}
