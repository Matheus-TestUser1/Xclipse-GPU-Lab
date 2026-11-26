package dev.xclipse.gpulab;
class ArchitectureLabTest {public static void main(String[] args){
 String a="[TEST] FP16 |\nasic(MGFX2)\nsgpr_count(6)\nvgpr_count(8)\nwave_size(32)\n s_mov_b32 s0, s2 // 00000004: BE800002\n v_pk_fma_f16 v0, v1, v2, v3 // 00000008: CC000000 00000000\n";
 String seed="32|s_mov_b32 s0, s2|BE800002\n";
 String ref="32\ts_mov_b32 s0, s2\tBE800002\tBE800002\tASSEMBLED\ts_mov_b32 s0, s2\n";
 if(!ArchitectureLab.database(seed,a).contains("Distinct mnemonics=2")||!ArchitectureLab.reference(seed,a,ref).contains("SUMMARY exact=1")||!ArchitectureLab.registers(a).contains("wave=32 SGPR=6 VGPR=8"))throw new AssertionError("architecture report");
 System.out.println("PASS architecture evidence");
}}
