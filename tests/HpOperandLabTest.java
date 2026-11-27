package dev.xclipse.gpulab;
class HpOperandLabTest {
 public static void main(String[] args){
  String raw="[TEST] HPO Fma half uniform push RTE16_RTE32 wave=64 | \nasic(MGFX2)\nwave_size(64)\n"+
   " v_fma_hp_mix_f32 v2, sel_lo(v1), sel_lo(s4), -v3 // 0040: CC1D4002 1BC1FF01\n"+
   " v_fma_hp_mixlo_f16 v1, abs(v1), sel_hi(v2), s5 // 0048: CC1E4001 13C1FF01\nFloatMode = 192;\n"+
   "HPO_ROW|matched_sources|0|41802000|4c01|dc02|39800000\nHPO_MODELS|phase=matched_sources|early_half_fused_RTE=64/64\nPASS HPO strict split phase=matched_sources\n";
  String result=HpOperandLab.summarize(raw);
  if(!result.contains("op1d_tests=1 op1e_tests=1 model_phases=1 failed_checks=0")||!result.contains("HPO_FLOAT_MODE|[192]")||!result.contains("VGPR=true|SGPR=true")||!result.contains("neg_text=true")||!result.contains("abs_text=true")||result.contains("HPO_ROW|matched"))throw new AssertionError(result);
  if(!HpOperandLab.summarize("").contains("Sem captura"))throw new AssertionError("empty");
  String skip=HpOperandLab.summarize("[TEST] HPO | SKIP missing feature\n");
  if(!skip.contains("tests=1 skipped=1 hp_emitted_tests=0")||!skip.contains("HP_NOT_EMITTED"))throw new AssertionError(skip);
  System.out.println("PASS HPO opcode, source-register, modifier and model evidence");
 }
}
