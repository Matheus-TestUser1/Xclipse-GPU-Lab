package dev.xclipse.gpulab;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.zip.*;
final class ReportBundle {
 static final class Entry {
  final String name; final File file; final String inline;
  Entry(String name,File file){this.name=name;this.file=file;this.inline=null;}
  Entry(String name,String inline){this.name=name;this.file=null;this.inline=inline;}
  InputStream open()throws IOException{return file!=null?new FileInputStream(file):new ByteArrayInputStream(inline.getBytes(StandardCharsets.UTF_8));}
 }
 static void write(OutputStream target,List<Entry> entries,boolean zip)throws IOException{
  if(entries.isEmpty())throw new IOException("No reports");
  StringBuilder manifest=new StringBuilder("Xclipse GPU Lab | Report bundle\nReports: "+entries.size()+"\n");
  if(zip){try(ZipOutputStream z=new ZipOutputStream(new BufferedOutputStream(target),StandardCharsets.UTF_8)){int i=0;for(Entry e:entries){String name=String.format(Locale.ROOT,"%03d-%s.txt",++i,e.name.replaceAll("[^a-zA-Z0-9._-]","_").replaceFirst("\\.txt$",""));z.putNextEntry(new ZipEntry(name));try(InputStream in=e.open()){copy(in,z);}z.closeEntry();manifest.append(name).append('\n');}z.putNextEntry(new ZipEntry("index.txt"));put(z,manifest.toString());z.closeEntry();}}
  else{try(BufferedOutputStream o=new BufferedOutputStream(target)){put(o,manifest.toString());for(Entry e:entries){put(o,"\n============================================================\n"+e.name+"\n============================================================\n");try(InputStream in=e.open()){copy(in,o);}put(o,"\n");}}}
 }
 static void put(OutputStream o,String s)throws IOException{o.write(s.getBytes(StandardCharsets.UTF_8));}
 static void copy(InputStream in,OutputStream out)throws IOException{byte[] b=new byte[65536];int n;while((n=in.read(b))!=-1)out.write(b,0,n);}
}
