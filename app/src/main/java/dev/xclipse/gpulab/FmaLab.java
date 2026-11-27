package dev.xclipse.gpulab;
import java.util.*;

/** Correlates shader observations with emitted instructions, without executing opcodes. */
final class FmaLab {
 static String gfx11(int op){switch(op){
  case 0x0e:return "v_pk_fma_f16";
  case 0x20:return "v_fma_mix_f32";
  case 0x21:return "v_fma_mixlo_f16";
  case 0x22:return "v_fma_mixhi_f16";
  case 0x40:return "v_wmma_f32_16x16x16_f16";
  case 0x41:return "v_wmma_f32_16x16x16_bf16";
  case 0x42:return "v_wmma_f16_16x16x16_f16";
  case 0x43:return "v_wmma_bf16_16x16x16_bf16";
  case 0x44:return "v_wmma_i32_16x16x16_iu8";
  case 0x45:return "v_wmma_i32_16x16x16_iu4";
  default:return "SEM REFERÊNCIA nesta tabela RDNA3";
 }}
 static String decode(ArchitectureLab.Sample sample){
  try{
   long word=Long.parseLong(sample.words.split(" ")[0],16);
   if((word>>>26)!=0x33)return "";
   int op=(int)((word>>>16)&127);
   return "wave="+sample.wave+" | "+sample.asm+" | words="+sample.words+" | OP[22:16]=0x"+Integer.toHexString(op)+" | GFX11="+gfx11(op);
  }catch(NumberFormatException e){return "";}
 }
 static String summarize(String raw){
  StringBuilder b=new StringBuilder("[Pesquisa FMA MGFX2 para RADV/ACO]\n");
  if(raw.isEmpty())return b.append("Sem captura. Execute FMA MGFX2 primeiro.\n").toString();
  b.append("O compilador escolhe as instruções. Cada resultado descreve o shader completo, não uma instrução isolada.\n")
   .append("fma() GLSL.std.450 não garante fusão; correspondência com fused/split é uma observação.\n")
   .append("RTE/RTZ só são solicitados quando anunciados. Controle split usa NoContraction.\n")
   .append("Perturbação: altera somente A da metade baixa da lane 0, reutilizando o mesmo pipeline.\n")
   .append("Não investiga NaN, infinito ou denormais; sinais de zero são equivalentes no comparador.\n\n");
  int tests=0,skipped=0,failures=0,modelRows=0;
  for(String line:raw.split("\n")){
   if(line.startsWith("[GPU]"))b.append(line).append('\n');
   if(line.startsWith("[TEST]")){tests++;if(line.contains("SKIP"))skipped++;b.append('\n').append(line).append('\n');}
   else if(line.startsWith("FMA_BUILD|")||line.startsWith("FMA_FEATURES|")||line.startsWith("SPIRV_FINGERPRINT|")||line.startsWith("FMA_MODELS|")||line.startsWith("FMA_WAVE|")||line.startsWith("PASS FMA")||line.startsWith("FAIL FMA")||line.startsWith("INCONCLUSIVE")){
    b.append(line).append('\n');if(line.startsWith("FAIL FMA"))failures++;if(line.startsWith("FMA_MODELS|"))modelRows++;
   }
  }
  b.append("\n[ISA FMA por teste: OP interpretado somente em VOP3P]\n");
  Set<String> emitted=new LinkedHashSet<>();
  for(String block:raw.split("(?m)(?=^\\[TEST\\])")){
   String testLabel=block.startsWith("[TEST]")?block.split("\\n",2)[0]:"sem teste";
   for(ArchitectureLab.Sample sample:ArchitectureLab.samples(block))if(sample.asm.startsWith("v_")&&(sample.asm.contains("fma")||sample.asm.contains("wmma")||sample.asm.contains("mfma"))){String row=decode(sample);if(row.isEmpty())row="wave="+sample.wave+" | "+sample.asm+" | words="+sample.words+" | fora de VOP3P; OP não interpretado";emitted.add(testLabel+"\n  "+row);}
  }
  for(String row:emitted)b.append(row).append('\n');
  if(emitted.isEmpty())b.append("Nenhuma FMA/WMMA reconhecida no formato VOP3P; consulte a ISA bruta.\n");
  b.append("\nSUMMARY tests=").append(tests).append(" skipped=").append(skipped).append(" model_rows=").append(modelRows).append(" failed_checks=").append(failures).append('\n');
  b.append("0x1e/variante hp exige semântica própria: não converter automaticamente em mixlo 0x21 nem em WMMA.\n")
   .append("Ausência de v_wmma não prova ausência física. Estes testes não exercitam uma operação matricial coletiva.\n")
   .append("Não é benchmark; não conclui ganho de desempenho ou equivalência entre waves.\n")
   .append("Referências: AMD RDNA3 ISA, seções 7.5 e 15.3.6; Khronos SPV_KHR_float_controls e VK_KHR_shader_fma.\n");
  return b.toString();
 }
}
