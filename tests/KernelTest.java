package dev.xclipse.gpulab;
import java.nio.charset.StandardCharsets;
class KernelTest {
 public static void main(String[] args){
  Kernel.Capture capture=new Kernel.Capture();byte[] chunk="x".repeat(4096).getBytes(StandardCharsets.UTF_8);
  for(int i=0;i<128;i++)capture.append(chunk,chunk.length);
  if(capture.text().length()!=512*1024||capture.text().contains("TRUNCATED"))throw new AssertionError("exact limit");
  capture.append(chunk,chunk.length);
  if(!capture.text().endsWith("[OUTPUT TRUNCATED: 512 KiB]\n"))throw new AssertionError("truncation must be visible");
  System.out.println("PASS kernel bounded capture");
 }
}
