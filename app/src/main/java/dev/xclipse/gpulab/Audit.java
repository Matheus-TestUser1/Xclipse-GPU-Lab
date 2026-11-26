package dev.xclipse.gpulab;
import java.util.*;
final class Audit {
 static String summarize(String report){
  StringBuilder out=new StringBuilder("[Classificacao por GPU e estrutura de features]\n");
  String gpu="desconhecida"; int count=0; Set<String> advertised=new TreeSet<>(), audited=new HashSet<>(); boolean inExtensions=false;
  for(String line:(report+"\n[END]").split("\n")){
   if(line.startsWith("[GPU]")||line.equals("[END]")){
    for(String x:advertised)if(!audited.contains(x))out.append(gpu).append(" | ").append(x).append(" | ANUNCIADA / SEM AUDITORIA DE FEATURE\n");
    advertised.clear();audited.clear();inExtensions=false;gpu=line;continue;
   }
   if(line.startsWith("[Device extensions:")){inExtensions=true;continue;}
   if(line.startsWith("["))inExtensions=false;
   if(inExtensions&&line.matches("VK_[A-Za-z0-9_]+ r[0-9]+"))advertised.add(line.split(" ")[0]);
   if(!line.startsWith("AUDIT|"))continue;
   String[] p=line.split("\\|",-1); if(p.length!=6)continue;
   try{int yes=Integer.parseInt(p[3]),no=Integer.parseInt(p[4]),invalid=Integer.parseInt(p[5]);String s=invalid>0?"INCONSISTENTE: VkBool32 invalido":yes==0?"FEATURES CONSULTADAS DESATIVADAS":no>0?"PARCIAL (bits opcionais)":"BITS CONSULTADOS ATIVOS";
    out.append(gpu).append(" | ").append(p[1]).append(" | ").append(s).append(" | sim=").append(yes).append(" nao=").append(no).append(" invalidos=").append(invalid).append("\n");
    for(String owner:p[2].split(","))if(advertised.contains(owner))audited.add(owner);count++;
   }catch(NumberFormatException ignored){}
  }
  return out.append("Estruturas classificadas: ").append(count).append("\nPARCIAL nao significa violacao da especificacao: bits podem ser opcionais. Desativadas significa reportadas como false, nao fisicamente ausentes. Nenhuma operacao de shader foi validada por esta auditoria.\n\n").append(report).toString();
 }
}
