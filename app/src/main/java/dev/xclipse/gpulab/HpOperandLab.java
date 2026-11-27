package dev.xclipse.gpulab;
import java.util.*;
import java.util.regex.*;

/** Evidence from compiler-selected shaders. Register allocation is never inferred from GLSL. */
final class HpOperandLab {
 static final Pattern VECTOR=Pattern.compile("(?<![A-Za-z0-9_])v(?:[0-9]+|\\[[0-9]+:[0-9]+\\])");
 static final Pattern SCALAR=Pattern.compile("(?<![A-Za-z0-9_])s(?:[0-9]+|\\[[0-9]+:[0-9]+\\])");
 static String sources(String asm){
  int comma=asm.indexOf(',');String src=comma<0?"":asm.substring(comma+1);
  return "VGPR="+VECTOR.matcher(src).find()+"|SGPR="+SCALAR.matcher(src).find()+"|sel_lo="+src.contains("sel_lo")+"|sel_hi="+src.contains("sel_hi")+"|abs_text="+(src.contains("abs(")||src.contains("|"))+"|neg_text="+src.contains("-");
 }
 static String summarize(String raw){return summarize(raw,false);}
 static String summarize(String raw,boolean roles){
  String prefix=roles?"HPR":"HPO";
  StringBuilder b=new StringBuilder(roles?"[HP posições FP32 A/B/C: evidências para RADV/ACO]\n":"[HP operandos: evidências para RADV/ACO]\n");
  if(raw.isEmpty())return b.append(roles?"Sem captura. Execute HP posições FP32 / RADV.\n":"Sem captura. Execute HP operandos / RADV.\n").toString();
  b.append(roles?"180 configurações candidatas: A/B/C × 4 operações × 5 modos × wave padrão/32/64. A posição no GLSL pode mudar na ISA.\n":"81 configurações candidatas: 9 caminhos × 3 modos × wave padrão/32/64. Extensões e modos precisam ser anunciados.\n")
   .append(roles?"Fases baseline e perturb_selected_lane0: altera somente a fonte FP32 da lane zero na posição selecionada.\n":"Fases: varying_coefficients varia B/C por lane; uniform push mantém B/C uniformes. matched_sources iguala B/C em todos os caminhos para comparação.\n")
   .append("24 modelos produzem bits FP32: saída half expandida ou resultado FP32 completo, RTE32/RTZ32 e arredondamento misto entre conversão/produto/soma. São hipóteses sobre o shader inteiro; coincidência não prova unicidade.\n")
   .append("SGPR/VGPR abaixo são fontes reconhecidas no texto da ISA; push constants não garantem SGPR. Modificadores podem ser materializados por instruções separadas.\n")
   .append("Cancelamento inclui (16.015625 × 16.015625) − 256.5: fused=2^-12, split half=0 em RTE. Domínio finito, sem denormais/NaN/Inf; sinais de zero equivalentes.\n");
  int tests=0,skips=0,hpTests=0,op1d=0,op1e=0,phases=0,failures=0,inconclusive=0,unmodeled=0;
  for(String block:raw.split("(?m)(?=^\\[TEST\\])")){
   boolean test=block.startsWith("[TEST]");
   if(test){tests++;String label=block.split("\\n",2)[0];if(label.contains("SKIP"))skips++;b.append('\n').append(label).append('\n');}
   for(String line:block.split("\\n")){
    if(line.startsWith("[GPU]")||line.startsWith(prefix+"_")&&!line.startsWith(prefix+"_ROW|")&&!line.startsWith(prefix+"_ROWS|")||line.startsWith("SPIRV_FINGERPRINT|")||line.startsWith("PASS "+prefix)||line.startsWith("FAIL "+prefix)||line.startsWith("INCONCLUSIVE"))b.append(line).append('\n');
    if(line.startsWith(prefix+"_MODELS|"))phases++;
    if(line.startsWith("FAIL "+prefix))failures++;
    if(line.startsWith("INCONCLUSIVE"))inconclusive++;
   }
   for(String modelLine:block.split("\n"))if(modelLine.startsWith(prefix+"_MODELS|")){
    boolean full=false;for(String field:modelLine.split("\\|"))if(field.endsWith("=64/64"))full=true;
    if(!full){unmodeled++;b.append("OBSERVED_UNMODELED|").append(modelLine.split("\\|",3)[1]).append("|nenhum modelo correspondeu às 64 lanes; não é FAIL de conformidade\n");}
   }
   if(!test)continue;
   b.append(HpModeLab.analyze(block).describe());
   Set<String> modes=new LinkedHashSet<>();Matcher mode=HpLab.FLOAT_MODE.matcher(block);while(mode.find())modes.add(mode.group(1));
   if(!modes.isEmpty())b.append(prefix+"_FLOAT_MODE|").append(modes).append('\n');
   boolean hp=false,has1d=false,has1e=false;Set<String> isa=new LinkedHashSet<>();
   for(ArchitectureLab.Sample sample:ArchitectureLab.samples(block)){
    boolean isHp=HpLab.isHp(sample);String decoded=FmaLab.decode(sample);
    if(isHp){hp=true;has1d|=decoded.contains("OP[22:16]=0x1d ");has1e|=decoded.contains("OP[22:16]=0x1e ");}
    if(isHp||sample.asm.contains("fma")||sample.asm.startsWith("v_cvt")||sample.asm.contains("mul_f")||sample.asm.contains("add_f")||sample.asm.startsWith("v_and")||sample.asm.startsWith("v_xor")||sample.asm.startsWith("s_round_mode")){
     String row=decoded.isEmpty()?"wave="+sample.wave+" | "+sample.asm+" | words="+sample.words:decoded;
     if(sample.asm.contains("fma"))row+=" | ISA_SOURCES|"+sources(sample.asm);
     isa.add(row);
    }
   }
   if(hp)hpTests++;if(has1d)op1d++;if(has1e)op1e++;
   b.append(prefix+"_COVERAGE|hp_emitted=").append(hp).append("|op1d=").append(has1d).append("|op1e=").append(has1e).append('\n');
   for(String row:isa)b.append("  ").append(row).append('\n');
  }
  b.append("\nSUMMARY tests=").append(tests).append(" skipped=").append(skips).append(" hp_emitted_tests=").append(hpTests).append(" op1d_tests=").append(op1d).append(" op1e_tests=").append(op1e).append(" model_phases=").append(phases).append(" failed_checks=").append(failures).append(" inconclusive=").append(inconclusive).append(" unmodeled_phases=").append(unmodeled).append('\n');
  return b.append(hpTests==0?"HP_NOT_EMITTED: estes shaders não caracterizaram hp.\n":"HP_EMITTED: atribuir conclusões somente aos caminhos com hp documentado na própria ISA.\n")
   .append("0x1D/0x1E não são WMMA. Sem benchmark ou alteração de clocks/governor. Consulte as linhas ROW no relatório bruto para reproduzir cada modelo.\n").append(HpComparisons.summarize(raw)).toString();
 }
}
