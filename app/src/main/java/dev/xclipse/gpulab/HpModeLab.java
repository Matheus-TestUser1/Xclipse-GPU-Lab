package dev.xclipse.gpulab;
import java.util.*;
import java.util.regex.*;

/** Static reconstruction using the RDNA MODE layout, not hardware register readback. */
final class HpModeLab {
 static final Pattern LINE=Pattern.compile("^\\s*((?:s_|v_|ds_|buffer_|global_|flat_|image_|scratch_|tbuffer_|exp_)[a-z0-9_]+)\\s*(.*?)\\s*(?://|;)\\s*([0-9A-Fa-f]+):\\s*((?:[0-9A-Fa-f]{8}\\s*)+)$");
 static final Pattern FLOAT_MODE=Pattern.compile("^\\s*FloatMode\\s*=\\s*(\\d+)\\s*;?\\s*$",Pattern.MULTILINE);
 static final class Fma {String asm,opcode;int state;Fma(String a,String op,int s){asm=a;opcode=op;state=s;}}
 static final class Profile {
  String reason="";Integer initial;List<Fma> fmas=new ArrayList<>();List<String> changes=new ArrayList<>();
  Integer singleState(String opcode){return reason.isEmpty()&&fmas.size()==1&&fmas.get(0).opcode.equals(opcode)?fmas.get(0).state:null;}
  String describe(){
   StringBuilder b=new StringBuilder("HP_MODE_TRACE|status=").append(reason.isEmpty()?"INFERRED_LINEAR":"UNKNOWN").append("|initial_fp_mode=").append(initial==null?"?":hex(initial)).append("|reason=").append(reason).append('\n');
   for(String change:changes)b.append("HP_MODE_CHANGE|").append(change).append('\n');
   for(Fma f:fmas)b.append("HP_FMA_STATE|op=").append(f.opcode).append("|fp_mode=").append(reason.isEmpty()?hex(f.state):"?").append("|round32=").append(reason.isEmpty()?round(f.state&3):"?").append("|round16_64=").append(reason.isEmpty()?round((f.state>>2)&3):"?").append("|asm=").append(f.asm).append('\n');
   return b.toString();
  }
 }
 static String hex(int v){return String.format(Locale.ROOT,"0x%02x",v);}
 static String round(int n){return new String[]{"RTE","RTP","RTN","RTZ"}[n];}
 static String section(String block){
  int marker=block.indexOf("[AMD driver disassembly]");String s=marker<0?block:block.substring(marker+24);
  int end=s.indexOf("AMD statistics");if(end>=0)s=s.substring(0,end);
  if(marker<0){int asic=s.indexOf("asic(");if(asic<0)return "";s=s.substring(asic);int next=s.indexOf("asic(",5);if(next>=0)s=s.substring(0,next);}
  return s;
 }
 static Profile analyze(String block){
  Profile p=new Profile();String s=section(block);
  if(!s.contains("asic(MGFX2)")){p.reason="missing MGFX2 ISA";return p;}
  Set<Integer> initial=new LinkedHashSet<>();Matcher fm=FLOAT_MODE.matcher(s);
  while(fm.find())try{int v=Integer.parseInt(fm.group(1));if(v<0||v>255)throw new NumberFormatException();initial.add(v);}catch(NumberFormatException e){p.reason="invalid FloatMode";return p;}
  if(initial.size()!=1){p.reason="missing or conflicting FloatMode";return p;}
  p.initial=initial.iterator().next();int state=p.initial;long nextAddress=0;boolean any=false,ended=false;
  for(String line:s.split("\n")){
   Matcher m=LINE.matcher(line);
   if(!m.matches()){
    if(line.trim().matches("^(?:s_|v_|ds_|buffer_|global_|flat_|image_|scratch_|tbuffer_|exp_)[a-z0-9_]+\\s+.*"))p.reason="unparsed instruction";
    continue;
   }
   String op=m.group(1),operands=m.group(2);String[] words=m.group(4).trim().split("\\s+");long address;
   try{address=Long.parseLong(m.group(3),16);}catch(NumberFormatException e){p.reason="invalid instruction address";continue;}
   if(address!=nextAddress)p.reason="incomplete or nonsequential ISA";
   nextAddress=address+4L*words.length;any=true;
   if(ended&&!op.equals("s_code_end")&&!op.equals("s_nop"))p.reason="instructions after termination";
   if(op.startsWith("s_branch")||op.startsWith("s_cbranch")||op.contains("call")||op.contains("setpc")||op.contains("swappc")||op.contains("rfe")||op.contains("getreg")||op.contains("setreg"))p.reason="control flow or untracked register access";
   if(op.equals("s_round_mode")||op.equals("s_denorm_mode")){
    try{
     long word=Long.parseLong(words[0],16);int value=Integer.decode(operands.trim());long expected=op.equals("s_round_mode")?0xbf910000L:0xbf920000L;
     if(words.length!=1||value<0||value>15||(word&0xfffffff0L)!=expected||(word&15)!=value)throw new NumberFormatException();
     state=op.equals("s_round_mode")?(state&0xf0)|value:(state&15)|(value<<4);
     p.changes.add("pc=0x"+Long.toHexString(address)+"|"+op+"="+hex(value)+"|fp_mode="+hex(state));
    }catch(NumberFormatException e){p.reason="unrecognized mode write";}
   }
   if(op.contains("fma")){
    long word=Long.parseLong(words[0],16);String code=(word>>>26)==0x33?"0x"+Long.toHexString((word>>>16)&127):"non-VOP3P";
    p.fmas.add(new Fma(op+" "+operands,code,state));
   }
   if(op.equals("s_endpgm")||op.equals("s_endpgm_saved"))ended=true;
  }
  if(!any||!ended)p.reason="incomplete ISA or missing termination";
  return p;
 }
}
