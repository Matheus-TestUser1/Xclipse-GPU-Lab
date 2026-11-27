package dev.xclipse.gpulab;
class FmaLabTest {
 public static void main(String[] args){
  String capture="[GPU] Xclipse 940\n[TEST] FMA packed Fma default wave=32 | \nasic(MGFX2)\nwave_size(32)\n"+
   " v_pk_fma_f16 v2, v2, v1, v0 // 0058: CC0E0002 0BC00902\n"+
   " v_fma_hp_mixlo_f16 v2, v1, 1.0, 1.0 // 0060: CC1E4002 13C9E501\n"+
   "FMA_MODELS|phase=baseline|fused_RTE=128/128\nFMA_WAVE|requested=32|shader_reported=32|mismatches=0/64\n"+
   "PASS FMA lane independence changed_other_elements=0/127\n[TEST] FMA split RTZ16 wave=64 | SKIP missing feature\n";
  String result=FmaLab.summarize(capture);
  if(!result.contains("OP[22:16]=0x1e | GFX11=SEM REFERÊNCIA")||!result.contains("OP[22:16]=0xe | GFX11=v_pk_fma_f16")||!result.contains("tests=2 skipped=1 model_rows=1 failed_checks=0"))throw new AssertionError(result);
  if(!FmaLab.decode(new ArchitectureLab.Sample("32","v_fma_f16 v0, v1, v2, v3","D6480002 00000000")).isEmpty())throw new AssertionError("VOP3 is not VOP3P");
  if(!FmaLab.summarize("").contains("Sem captura"))throw new AssertionError("missing data");
  System.out.println("PASS FMA evidence and encoding boundaries");
 }
}
