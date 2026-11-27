package dev.xclipse.gpulab;
class HpLabTest {
 public static void main(String[] args){
  String hp="[TEST] HP FP32-to-half mul-add constants RTE16_RTE32 wave=64 | \nasic(MGFX2)\nwave_size(64)\n"+
   " v_fma_hp_mixlo_f16 v1, v1, 1.5, 0.5 // 004C: CC1E4001 13C1FF01\nFloatMode = 192;\n"+
   "HP_MODELS|phase=baseline|early_fused_RTE=64/64\nPASS HP lane independence changed_other_lanes=0/63\n";
  String result=HpLab.summarize(hp,"");
  if(!result.contains("HP_COVERAGE|hp_emitted=true")||!result.contains("hp_emitted_tests=1 model_rows=1 failed_checks=0")||!result.contains("HP_FLOAT_MODE|[192]")||!result.contains("OP[22:16]=0x1e"))throw new AssertionError(result);
  String no=HpLab.summarize("[TEST] HP control | SKIP missing feature\n",hp);
  if(!no.contains("HP_NOT_EMITTED")||!no.contains("tests=1 skipped=1 hp_emitted_tests=0")||!no.contains("v_fma_hp_mixlo_f16"))throw new AssertionError(no);
  if(!HpLab.summarize("",hp).contains("Sem captura"))throw new AssertionError("empty capture");
  System.out.println("PASS HP coverage, FloatMode, separate legacy evidence");
 }
}
