package dev.xclipse.gpulab;
import java.util.*;
import java.util.regex.*;

/** Compares outputs only when GPU, mode, phase, lane and input bits match. */
final class HpComparisons {
 static final Pattern LABEL=Pattern.compile("^\\[TEST\\] (HPO|HPR) (.*?) (default|RTE16_RTE32|RTZ16_RTE32) wave=(default|32|64) \\|.*$");
 static final Pattern WORD=Pattern.compile("//.*?:\\s*(CC[0-9A-Fa-f]{6})(?:\\s|$)");
 static final String[] HPO={"Fma half per-lane lower","Fma half uniform push","Fma half negate A","Fma half abs A negate C","mul-add half per-lane","mul-add half uniform push","Fma promoted half keep FP32","split half NoContraction","Fma half per-lane upper"};
 static final String[] OPS={"Fma half","mul-add half","Fma promoted keep FP32","split half NoContraction"};
 static final class Row {String inputs,phase;long value;Row(String i,String p,long v){inputs=i;phase=p;value=v;}}
 static final class Capture {
  String gpu,prefix,kind,mode,wave;boolean invalid;Map<String,Row> rows=new LinkedHashMap<>();Set<String> ops=new TreeSet<>();
  String key(){return gpu+"|"+prefix+"|"+kind+"|"+mode+"|"+wave;}
 }
 static final class Stats {int tests,compared,different,unmatched,nonfinite;long maxUlp;Set<String> left=new TreeSet<>(),right=new TreeSet<>();}
 static long ordered(long v){return (v&0x80000000L)!=0?(~v)&0xffffffffL:v|0x80000000L;}
 static boolean finite(long v){return (v&0x7f800000L)!=0x7f800000L;}
 static boolean equal(long a,long b){return a==b||((a|b)&0x7fffffffL)==0;}
 static String summarize(String raw){
  Map<String,Capture> captures=new LinkedHashMap<>();String gpu="unknown";int gpuIndex=-1,malformed=0;Capture current=null;
  for(String line:raw.split("\n")){
   if(line.startsWith("[GPU]")){gpu=(++gpuIndex)+":"+line.substring(5).trim();current=null;}
   if(line.startsWith("[TEST]")){
    current=null;Matcher m=LABEL.matcher(line);
    if(m.matches()&&!line.contains("SKIP")){
     current=new Capture();current.gpu=gpu;current.prefix=m.group(1);current.kind=m.group(2);current.mode=m.group(3);current.wave=m.group(4);
     if(captures.putIfAbsent(current.key(),current)!=null){current=null;malformed++;}
    }
   }
   if(current==null)continue;
   if(line.contains("fma")){Matcher word=WORD.matcher(line);if(word.find())current.ops.add("0x"+Long.toHexString((Long.parseLong(word.group(1),16)>>>16)&127));}
   if(!line.startsWith(current.prefix+"_ROW|"))continue;
   try{
    String[] p=line.split("\\|");int valueIndex=current.prefix.equals("HPO")?6:9;
    if(p.length<=valueIndex)throw new IllegalArgumentException();
    int lane=Integer.parseInt(p[2]);if(lane<0||lane>=64)throw new IllegalArgumentException();
    StringBuilder inputs=new StringBuilder();
    for(int i=3;i<valueIndex;i++){long v=Long.parseLong(p[i],16);if(v<0||v>0xffffffffL)throw new IllegalArgumentException();inputs.append(Long.toHexString(v)).append('/');}
    long v=Long.parseLong(p[valueIndex],16);if(v<0||v>0xffffffffL)throw new IllegalArgumentException();
    if(current.rows.putIfAbsent(p[1]+"|"+lane,new Row(inputs.toString(),p[1],v))!=null)throw new IllegalArgumentException();
   }catch(IllegalArgumentException e){current.invalid=true;malformed++;}
  }
  Map<String,Stats> stats=new LinkedHashMap<>();int missingPartners=0;
  for(Capture c:captures.values()){
   if(c.invalid||c.rows.isEmpty())continue;
   if(c.wave.equals("32")){
    Capture other=find(captures,c,c.kind,"64");
    if(other==null)missingPartners++;else compare(stats,"wave32_vs64",c,other,false);
   }
   if(c.prefix.equals("HPO")){
    int kind=Arrays.asList(HPO).indexOf(c.kind),target=-1;String category="";boolean matched=false;
    if(kind==0){target=1;category="lane_vs_uniform_Fma";matched=true;}
    if(kind==4){target=5;category="lane_vs_uniform_muladd";matched=true;}
    if(target>=0){Capture other=find(captures,c,HPO[target],c.wave);if(other==null)missingPartners++;else compare(stats,category,c,other,matched);}
    if(kind==0){Capture other=find(captures,c,HPO[8],c.wave);if(other==null)missingPartners++;else compare(stats,"lower_vs_upper",c,other,true);}
    if(kind==4){Capture other=find(captures,c,HPO[6],c.wave);if(other==null)missingPartners++;else compare(stats,"muladd_vs_promoted_FP32",c,other,false);}
   }else if(c.kind.matches("FP32_[ABC] mul-add half")){
    String otherKind=c.kind.replace("mul-add half","Fma promoted keep FP32");Capture other=find(captures,c,otherKind,c.wave);
    if(other==null)missingPartners++;else compare(stats,c.kind.substring(0,6)+"_muladd_vs_promoted_FP32",c,other,false);
   }
  }
  StringBuilder b=new StringBuilder("\n[Comparações numéricas automáticas]\nMesma GPU, modo, fase, lane e bits de entrada. Diferenças são observações, não falhas de conformidade ou medições de desempenho. OPs abaixo vêm das linhas de FMA na ISA; não identificam sozinhos toda a semântica.\n");
  for(Map.Entry<String,Stats> e:stats.entrySet()){
   Stats s=e.getValue();b.append("HP_COMPARE|").append(e.getKey()).append("|pairs=").append(s.tests).append("|compared=").append(s.compared).append("|different=").append(s.different).append("|max_ulp=").append(s.maxUlp).append("|unmatched=").append(s.unmatched).append("|nonfinite=").append(s.nonfinite).append("|left_ops=").append(s.left).append("|right_ops=").append(s.right).append('\n');
  }
  if(stats.isEmpty())b.append("Sem pares comparáveis nesta captura.\n");
  return b.append("HP_COMPARE_STATUS|missing_partners=").append(missingPartners).append("|malformed_rows_or_tests=").append(malformed).append('\n').toString();
 }
 static Capture find(Map<String,Capture> all,Capture c,String kind,String wave){return all.get(c.gpu+"|"+c.prefix+"|"+kind+"|"+c.mode+"|"+wave);}
 static void compare(Map<String,Stats> stats,String category,Capture a,Capture b,boolean matchedOnly){
  if(b.invalid||b.rows.isEmpty())return;
  Stats s=stats.computeIfAbsent(category+"|gpu="+a.gpu+"|mode="+a.mode,k->new Stats());s.tests++;s.left.addAll(a.ops);s.right.addAll(b.ops);
  Set<String> keys=new LinkedHashSet<>(a.rows.keySet());keys.addAll(b.rows.keySet());
  for(String key:keys){
   Row x=a.rows.get(key),y=b.rows.get(key);if(matchedOnly&&!key.startsWith("matched_sources|"))continue;
   if(x==null||y==null||!x.inputs.equals(y.inputs)){s.unmatched++;continue;}
   if(!finite(x.value)||!finite(y.value)){s.nonfinite++;continue;}
   s.compared++;
   if(!equal(x.value,y.value)){s.different++;s.maxUlp=Math.max(s.maxUlp,Math.abs(ordered(x.value)-ordered(y.value)));}
  }
 }
}
