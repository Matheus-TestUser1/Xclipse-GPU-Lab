package dev.xclipse.gpulab;
import java.io.*;
import java.util.*;
import java.nio.charset.StandardCharsets;
final class Elf {
 static final class Info {
  final Set<String> needed=new TreeSet<>(), exports=new TreeSet<>(), imports=new TreeSet<>(), symbols=new TreeSet<>(); String soname=""; int bits,machine; boolean sections; int omitted;
  String render(){StringBuilder s=new StringBuilder("ELF "+bits+" bits machine="+machine+" sections="+sections+"\nSONAME="+soname+"\n"); for(String n:needed)s.append("NEEDED ").append(n).append('\n'); for(String n:exports)s.append("EXPORT ").append(n).append('\n');for(String n:imports)s.append("IMPORT ").append(n).append('\n');for(String n:symbols)s.append("SYMBOL ").append(n).append('\n');if(omitted>0)s.append("Symbol limit reached; omitted=").append(omitted).append('\n');return s.toString();}
 }
 static final class S {long type,off,size,link,ent;}
 static final class Reader implements Closeable {
  final RandomAccessFile f; final long length; boolean little,is64;
  Reader(File p)throws IOException{f=new RandomAccessFile(p,"r");length=f.length();}
  void check(long p,long n)throws IOException{if(p<0||n<0||p>length||n>length-p)throw new IOException("ELF range invalid");}
  long u(long p,int n)throws IOException{check(p,n);f.seek(p);long x=0;for(int i=0;i<n;i++){int b=f.readUnsignedByte();x|=(long)b<<(8*(little?i:n-1-i));}return x;}
  String str(S tab,long p)throws IOException{if(p<0||p>=tab.size)return "";long n=Math.min(1024,tab.size-p);check(tab.off+p,n);f.seek(tab.off+p);ByteArrayOutputStream b=new ByteArrayOutputStream();for(int i=0;i<n;i++){int c=f.readUnsignedByte();if(c==0)return b.toString("UTF-8");b.write(c);}return "";}
  S section(long p)throws IOException{S s=new S();s.type=u(p+4,4);s.off=u(p+(is64?24:16),is64?8:4);s.size=u(p+(is64?32:20),is64?8:4);s.link=u(p+(is64?40:24),4);s.ent=u(p+(is64?56:36),is64?8:4);return s;}
  public void close()throws IOException{f.close();}
 }
 static Info read(File p)throws IOException{
  try(Reader r=new Reader(p)){if(r.length<16||r.u(0,1)!=127||r.u(1,1)!=69||r.u(2,1)!=76||r.u(3,1)!=70)throw new IOException("not ELF");long cl=r.u(4,1),en=r.u(5,1);if((cl!=1&&cl!=2)||(en!=1&&en!=2))throw new IOException("unsupported ELF encoding");r.is64=cl==2;r.little=en==1;Info out=new Info();out.bits=r.is64?64:32;out.machine=(int)r.u(18,2);
   long sh=r.u(r.is64?40:32,r.is64?8:4), ents=r.u(r.is64?58:46,2), count=r.u(r.is64?60:48,2);
   if(sh==0||ents<(r.is64?64:40))return out;
   if(count==0)count=r.section(sh).size;
   if(count<1||count>16384||sh<0||sh>r.length||count>(r.length-sh)/ents)throw new IOException("section table invalid");
   S[] ss=new S[(int)count];for(int i=0;i<ss.length;i++)ss[i]=r.section(sh+i*ents);out.sections=true;
   for(S s:ss){if(s.type!=2&&s.type!=11&&s.type!=6)continue;if(s.link>=ss.length||s.link<0)continue;S str=ss[(int)s.link];if(str.type!=3)continue;r.check(str.off,str.size);r.check(s.off,s.size);
    int min=s.type==6?(r.is64?16:8):(r.is64?24:16);if(s.ent<min||s.ent>s.size)continue;long n=Math.min(500000,s.size/s.ent);
    for(long i=0;i<n;i++){long o=s.off+i*s.ent;if(s.type==6){long tag=r.u(o,r.is64?8:4);if(tag==0)break;if(tag==1||tag==14){String x=r.str(str,r.u(o+(r.is64?8:4),r.is64?8:4));if(tag==1&&!x.isEmpty())out.needed.add(x);if(tag==14)out.soname=x;}continue;}
     long idx=r.u(o,4);if(idx==0)continue;int info=(int)r.u(o+(r.is64?4:12),1),vis=(int)r.u(o+(r.is64?5:13),1)&3;long sec=r.u(o+(r.is64?6:14),2);
     String name=r.str(str,idx);if(name.isEmpty())continue;
     Set<String> dest=s.type==11?((sec==0)?out.imports:((info>>4)==1||(info>>4)==2||(info>>4)==10)&&(vis==0||vis==3)?out.exports:out.symbols):out.symbols;
     if(dest.size()<6000)dest.add(name);else out.omitted++;
    }
   }return out;
  }
 }
}
