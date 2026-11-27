package dev.xclipse.gpulab;
class HpComparisonsTest {
 static String hpo(String kind,String wave,boolean reference){
  return "[TEST] HPO "+kind+" RTZ16_RTE32 wave="+wave+" | \nwave_size(32)\n"+
   (reference?" v_fma_mix_f32 v2, v1, v2, v3 // 0040: CC204002 1C0E0302\n":" v_fma_hp_mix_f32 v2, v1, v2, v3 // 0040: CC1D4802 1C0E0501\n")+
   "HPO_ROW|varying_coefficients|0|bfd92c16|bc01|5800|"+(reference?"4301b2ad":"4301b2ac")+"\n"+
   "HPO_ROW|varying_coefficients|1|3f800000|3c00|0|"+(reference?"0":"80000000")+"\n"+
   "HPO_ROW|varying_coefficients|2|3f800000|"+(reference?"3c01":"3c00")+"|0|3f800000\n"+
   "HPO_ROW|varying_coefficients|3|3f800000|3c00|0|"+(reference?"3f800000":"7fc00000")+"\n";
 }
 public static void main(String[] args){
  String raw="[GPU] One\n"+hpo("mul-add half per-lane","32",false)+hpo("Fma promoted half keep FP32","32",true)+hpo("mul-add half per-lane","64",false);
  String s=HpComparisons.summarize(raw);
  if(!s.contains("compared=2|different=1|max_ulp=1|unmatched=1|nonfinite=1|left_ops=[0x1d]|right_ops=[0x20]")||!s.contains("wave32_vs64")||!s.contains("compared=3|different=0|max_ulp=0"))throw new AssertionError(s);
  String multiple=HpComparisons.summarize(raw+"[GPU] Two\n"+hpo("Fma promoted half keep FP32","32",true));
  if(!multiple.contains("gpu=0:One")||multiple.contains("gpu=1:Two|mode="))throw new AssertionError(multiple);
  String malformed=HpComparisons.summarize(raw+"HPO_ROW|varying_coefficients|0|bad|bad|bad|ffffffff\n");
  if(!malformed.contains("malformed_rows_or_tests=1"))throw new AssertionError(malformed);
  String role="[GPU] One\n[TEST] HPR FP32_C mul-add half RTZ16_RTE32 wave=64 | \nHPR_ROW|baseline|0|41802000|41802000|c3804000|4c01|4c01|dc02|39800000\n"+
   "[TEST] HPR FP32_C Fma promoted keep FP32 RTZ16_RTE32 wave=64 | \nHPR_ROW|baseline|0|41802000|41802000|c3804000|4c01|4c01|dc02|39800000\nHPR_MODELS|phase=baseline|early_half_fused_RTE=64/64\n";
  String rs=HpOperandLab.summarize(role,true);
  if(!rs.contains("180 configurações")||!rs.contains("HPR_COVERAGE")||!rs.contains("FP32_C_muladd_vs_promoted_FP32")||!rs.contains("compared=1|different=0"))throw new AssertionError(rs);
  String unmodeled=HpOperandLab.summarize("[TEST] HPO control default wave=64 | \nHPO_MODELS|phase=baseline|foo=63/64\n");
  if(!unmodeled.contains("OBSERVED_UNMODELED|phase=baseline"))throw new AssertionError(unmodeled);
  System.out.println("PASS HP comparison input matching, FP32 ULP, signed zeros, nonfinite, malformed rows, GPU isolation, role reports");
 }
}
