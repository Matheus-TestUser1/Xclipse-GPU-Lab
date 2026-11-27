#!/usr/bin/env python3
"""Inventory static FMA, WMMA and scheduling instructions; no encoding injection."""
import argparse,csv,re,sys
from collections import Counter
def audit(data):
    for section in re.split(r'(?=^\[TEST\] )',data,flags=re.M):
        if not section.startswith("[TEST] "):continue
        label=section.splitlines()[0][7:].split(" |",1)[0]
        fingerprint=re.search(r'SPIRV_FINGERPRINT\|fnv1a64=([0-9a-f]+)',section)
        amd=re.search(r'\[AMD driver disassembly\]\n(.*?)(?=^AMD statistics|^Executable |^HPR_ROWS|^HPE_ROWS|^HPB_CONFIG|\Z)',section,re.M|re.S)
        source="AMD" if amd else "UNKNOWN";code=amd.group(1) if amd else ""
        if not amd:
            for ir in re.finditer(r'^IR (.*?) text=1 bytes=\d+\n(.*?)(?=^IR |^Executable |^HPR_ROWS|^HPE_ROWS|^HPB_CONFIG|\Z)',section,re.M|re.S):
                if re.search(r'disassembl|assembly|aco',ir.group(1),re.I):
                    source="KHR:"+ir.group(1);code=ir.group(2);break
        ops=Counter(re.findall(r'\b(?:v_(?:fma|pk_fma|wmma|mfma)\w*|s_(?:delay_alu|singleuse_vdst|round_mode|denorm_mode))\b',code))
        words=re.findall(r'\b(v_fma_hp_\w+)\b[^\n]*?//\s*[0-9a-fA-F]+:\s*([0-9a-fA-F]{8})',code)
        yield dict(test=label,spirv=fingerprint.group(1) if fingerprint else "",source=source,
            hp1d=ops["v_fma_hp_mix_f32"],hp1e=ops["v_fma_hp_mixlo_f16"],mix=ops["v_fma_mix_f32"],
            packed=ops["v_pk_fma_f16"],wmma=sum(v for k,v in ops.items() if k.startswith("v_wmma")),
            delay=ops["s_delay_alu"],singleuse=ops["s_singleuse_vdst"],round_mode=ops["s_round_mode"],denorm_mode=ops["s_denorm_mode"],
            hp_words=";".join(sorted(set(n+":"+w for n,w in words))),observation="not_available" if not code else "static_text_only")
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument("report",nargs="?");ap.add_argument("--self-test",action="store_true");a=ap.parse_args()
    if a.self_test:
        fixture="[TEST] sample | \n[AMD driver disassembly]\n v_fma_hp_mixlo_f16 v2,v1 // 00000058: CC1E4002 140E0501\n s_delay_alu instid0(VALU_DEP_1)\nAMD statistics VGPR=4\nIR ACO text=1 bytes=9\nv_fma_hp_mixlo_f16\n"
        r=list(audit(fixture))[0];assert r["hp1e"]==1 and r["delay"]==1 and "CC1E4002" in r["hp_words"]
        assert list(audit("[TEST] empty | SKIP\n"))[0]["observation"]=="not_available"
        print("audit self-test PASS");return
    if not a.report:ap.error("report required")
    with open(a.report,encoding="utf-8") as f:rows=list(audit(f.read()))
    if not rows:return
    w=csv.DictWriter(sys.stdout,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
if __name__=="__main__":main()
