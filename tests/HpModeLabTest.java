package dev.xclipse.gpulab;
class HpModeLabTest {
 static String shader(int initial,boolean hp,String write){
  int at=0;StringBuilder s=new StringBuilder("[AMD driver disassembly]\nasic(MGFX2)\nwave_size(64)\n");
  if(!write.isEmpty()){s.append(" s_round_mode ").append(write).append(" // 0000: BF91000").append(write.substring(2).toUpperCase()).append('\n');at=4;}
  s.append(hp?" v_fma_hp_mix_f32 v2, v1, v2, v3":" v_fma_mix_f32 v2, v1, v2, v3").append(String.format(" // %04X: ",at)).append(hp?"CC1D4002 1C0E0501":"CC204002 1C0E0302").append('\n');
  s.append(String.format(" s_endpgm 0 // %04X: BFB00000\n",at+8)).append("FloatMode = ").append(initial).append(";\nAMD statistics VGPR=4\n");return s.toString();
 }
 static String capture(String kind,String mode,String asm){
  return "[TEST] HPR FP32_A "+kind+" "+mode+" wave=64 | \n"+asm+"HPR_ROW|baseline|0|41802000|41802000|c3804000|4c01|4c01|dc02|39800000\n";
 }
 static void check(boolean ok,String msg){if(!ok)throw new AssertionError(msg);}
 public static void main(String[] args){
  HpModeLab.Profile hp=HpModeLab.analyze(shader(204,true,"0xf")),mix=HpModeLab.analyze(shader(204,false,""));
  check(hp.singleState("0x1d")==207&&mix.singleState("0x20")==204,"dynamic MODE must override initial metadata");
  check(hp.describe().contains("round32=RTZ|round16_64=RTZ")&&mix.describe().contains("round32=RTE|round16_64=RTZ"),"decode precision fields");
  String raw="[GPU] Test\n"+capture("mul-add half","RTZ16_RTE32",shader(204,true,"0xf"))+capture("Fma promoted keep FP32","RTZ16_RTE32",shader(204,false,""));
  String compared=HpComparisons.summarize(raw);
  check(compared.contains("same_fp_mode_pairs=0|different_fp_mode_pairs=1|unknown_fp_mode_pairs=0|same_fp_mode_compared=0"),compared);
  raw="[GPU] Test\n"+capture("mul-add half","RTZ16_RTZ32",shader(207,true,""))+capture("Fma promoted keep FP32","RTZ16_RTZ32",shader(207,false,""));
  compared=HpComparisons.summarize(raw);
  check(compared.contains("same_fp_mode_pairs=1|different_fp_mode_pairs=0|unknown_fp_mode_pairs=0|same_fp_mode_compared=1|same_fp_mode_different=0"),compared);
  check(HpModeLab.analyze(shader(204,true,"0xf").replace("FloatMode = 204;","")).singleState("0x1d")==null,"missing initial metadata");
  check(HpModeLab.analyze(shader(204,true,"0xf").replace("s_round_mode 0xf","s_round_mode 0xc")).singleState("0x1d")==null,"mode mnemonic/encoding mismatch");
  check(HpModeLab.analyze(shader(204,true,"0xf").replace("0004:","0008:")).singleState("0x1d")==null,"ISA gap");
  check(HpModeLab.analyze(shader(204,true,"0xf").replace(" s_endpgm 0", " s_cbranch_scc0 0")).singleState("0x1d")==null,"branch/missing termination");
  check(HpModeLab.analyze(shader(204,true,"0xf").replace("FloatMode = 204;","FloatMode = 204;\nFloatMode = 207;")).singleState("0x1d")==null,"conflicting metadata");
  check(HpModeLab.analyze(shader(204,true,"0xf").replace("asic(MGFX2)","asic(UNKNOWN)")).singleState("0x1d")==null,"unknown target");
  String denorm=shader(192,true,"0xf").replace("s_round_mode 0xf // 0000: BF91000F","s_denorm_mode 0x0 // 0000: BF920000");
  check(HpModeLab.analyze(denorm).singleState("0x1d")==0,"track denorm writes too");
  check(HpModeLab.analyze(shader(204,true,"0xf")+shader(192,false,"")).singleState("0x1d")==207,"ignore second disassembly after AMD stats");
  check(HpModeLab.analyze(shader(204,true,"0xf").replace(" s_round_mode 0xf"," s_setreg_b32 hwreg(HW_REG_MODE), s0")).singleState("0x1d")==null,"untracked register write");
  String branch=shader(204,true,"0xf").replace("s_round_mode 0xf","s_cbranch_scc0 0");
  check(HpModeLab.analyze(branch).singleState("0x1d")==null,"branch despite valid termination");
  String two=shader(204,true,"").replace(" s_endpgm 0 // 0008: BFB00000", " v_fma_mix_f32 v2, v1, v2, v3 // 0008: CC204002 1C0E0302\n s_endpgm 0 // 0010: BFB00000");
  check(HpModeLab.analyze(two).singleState("0x1d")==null,"multiple FMA instructions cannot isolate an opcode pair");
  String unknown="[GPU] Test\n"+capture("mul-add half","RTE16_RTZ32","")+capture("Fma promoted keep FP32","RTE16_RTZ32","");
  check(HpComparisons.summarize(unknown).contains("unknown_fp_mode_pairs=1"),"missing ISA must stay unknown");
  System.out.println("PASS HP initial/dynamic MODE, same/different/unknown pairs, ISA gaps, control flow, target and encoding guards");
 }
}
