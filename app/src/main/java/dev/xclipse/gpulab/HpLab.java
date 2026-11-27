package dev.xclipse.gpulab;
import java.util.*;
import java.util.regex.*;

final class HpLab {
 static final Pattern FLOAT_MODE=Pattern.compile("^\\s*FloatMode\\s*=\\s*(\\d+)",Pattern.MULTILINE);
 static boolean isHp(ArchitectureLab.Sample s){return s.asm.startsWith("v_fma_hp_");}
 static String summarize(String raw,String legacy){
  StringBuilder b=new StringBuilder("[HP FP32→FP16: evidências para RADV/ACO]\n");
  if(raw.isEmpty())return b.append("Sem captura. Execute HP FP32→FP16 primeiro.\n").toString();
  b.append("A semântica é observada no shader completo. Só atribuir resultados à variante hp quando sua emissão estiver confirmada no mesmo teste.\n")
   .append("early: conversão para half antes da conta. late: conversão somente no final. Modelos fused32/split32 assumem RTE32; ideal não arredonda intermediários em FP32.\n")
   .append("RTE16_RTE32 e RTZ16_RTE32 são solicitados apenas com suporte anunciado e independência compatível. O modo padrão é observacional.\n")
   .append("Mais de um modelo pode coincidir; uma correspondência completa não estabelece unicidade.\n")
   .append("Inputs/resultados finitos normais ou zero; sem conclusões para NaN/Inf/denormais. Sem benchmark ou operação matricial coletiva.\n");
  int tests=0,skipped=0,hpTests=0,models=0,failed=0,inconclusive=0;
  for(String block:raw.split("(?m)(?=^\\[TEST\\])")){
   boolean test=block.startsWith("[TEST]");String label=block.split("\\n",2)[0];
   if(test){tests++;if(label.contains("SKIP"))skipped++;b.append('\n').append(label).append('\n');}
   for(String line:block.split("\\n")){
    if(line.startsWith("HP_BUILD|")||line.startsWith("HP_FEATURES|")||line.startsWith("SPIRV_FINGERPRINT|")||line.startsWith("HP_MODELS|")||line.startsWith("HP_WAVE|")||line.startsWith("PASS HP")||line.startsWith("FAIL HP")||line.startsWith("INCONCLUSIVE"))b.append(line).append('\n');
    if(line.startsWith("HP_MODELS|"))models++;
    if(line.startsWith("FAIL HP"))failed++;
    if(line.startsWith("INCONCLUSIVE"))inconclusive++;
   }
   if(!test)continue;
   Set<String> modes=new LinkedHashSet<>();Matcher mode=FLOAT_MODE.matcher(block);while(mode.find())modes.add(mode.group(1));
   if(!modes.isEmpty())b.append("HP_FLOAT_MODE|").append(modes).append('\n');
   Set<String> isa=new LinkedHashSet<>();boolean hp=false;
   for(ArchitectureLab.Sample s:ArchitectureLab.samples(block)){
    if(isHp(s))hp=true;
    if(s.asm.contains("fma")||s.asm.startsWith("v_cvt")||s.asm.contains("mul_f")||s.asm.contains("add_f")||s.asm.startsWith("s_round_mode")||s.asm.startsWith("s_denorm_mode")){
     String decoded=FmaLab.decode(s);isa.add(decoded.isEmpty()?"wave="+s.wave+" | "+s.asm+" | words="+s.words:decoded);
    }
   }
   if(hp)hpTests++;
   b.append("HP_COVERAGE|hp_emitted=").append(hp).append("|recognized_isa_samples=").append(isa.size()).append('\n');
   for(String line:isa)b.append("  ").append(line).append('\n');
  }
  b.append("\nSUMMARY tests=").append(tests).append(" skipped=").append(skipped).append(" hp_emitted_tests=").append(hpTests).append(" model_rows=").append(models).append(" failed_checks=").append(failed).append(" inconclusive=").append(inconclusive).append('\n');
  if(hpTests==0)b.append("HP_NOT_EMITTED: a bateria não caracterizou a variante hp; resultados de outras FMA não são substitutos dessa evidência.\n");
  else b.append("HP_EMITTED: correlacionar cada caminho, modo FloatMode e modelos; emissão não prova que hp seja a única instrução que influencia o resultado.\n");
  b.append("\n[Reprodutores anteriores: somente contexto, não validação do corpus novo]\n");
  int old=0;for(String block:legacy.split("(?m)(?=^\\[TEST\\])"))if(block.startsWith("[TEST]"))for(ArchitectureLab.Sample s:ArchitectureLab.samples(block))if(isHp(s)){b.append(block.split("\\n",2)[0]).append(" | ").append(s.asm).append(" | ").append(s.words).append('\n');old++;}
  if(old==0)b.append("Sem reprodutor hp anterior nesta sessão.\n");
  return b.append("\nNão equiparar automaticamente hp 0x1e a mixlo 0x21 ou WMMA. Opções e resultados não anunciados não são forçados.\n").toString();
 }
}
