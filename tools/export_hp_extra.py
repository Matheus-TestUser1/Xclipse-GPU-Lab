#!/usr/bin/env python3
"""Export observational v0.14 HPE/HPB rows from a report ZIP or TXT."""
import argparse, csv, re, zipfile

def classify(bits,width):
    eb,fb=(8,23) if width==32 else (5,10)
    e=(bits>>fb)&((1<<eb)-1);f=bits&((1<<fb)-1)
    sign="-" if bits>>(width-1) else "+"
    return sign+("zero" if not e and not f else "subnormal" if not e else "inf" if e==(1<<eb)-1 and not f else "nan" if e==(1<<eb)-1 else "normal")

def parse(raw):
    gpu="unknown"
    for section in re.split(r'(?=^\[GPU\] |^\[TEST\] )',raw,flags=re.M):
        if section.startswith("[GPU] "):gpu=section.splitlines()[0][6:].strip();continue
        if not section.startswith("[TEST] HP"):continue
        label=section.splitlines()[0][7:].split(" |",1)[0].strip()
        if not label.startswith(("HPE ","HPB ")):continue
        # Prefer first AMD disassembly; avoid duplicate KHR copy. If no AMD
        # disassembly, leave opcode evidence unknown rather than guess.
        isa=section.split("[AMD driver disassembly]",1)[1] if "[AMD driver disassembly]" in section else ""
        isa=re.split(r'^\s*(?:AMD statistics|Executable |HPE_ROWS|HPB_CONFIG)',isa,1,flags=re.M)[0]
        ops=re.findall(r'^\s*(v_(?:fma|pk_fma)\w*)\s+.*?//\s*[0-9a-fA-F]+:\s*([0-9a-fA-F]{8})',isa,re.M)
        opcodes=";".join(sorted(set(name+":"+word for name,word in ops)))
        fingerprint=re.search(r'SPIRV_FINGERPRINT\|fnv1a64=([0-9a-f]+)',section)
        common=dict(gpu=gpu,test=label,fingerprint=fingerprint.group(1) if fingerprint else "",observed_fma_words=opcodes,isa_source="AMD" if isa else "UNKNOWN")
        if label.startswith("HPE "):
            seen=set()
            for line in section.splitlines():
                if not line.startswith("HPE_ROW|"):continue
                p=line.split("|")
                if len(p)!=11:raise ValueError("Malformed HPE row: "+line)
                lane=int(p[1]);values=[int(v,16) for v in p[2:10]]
                if not 0<=lane<64 or lane in seen:raise ValueError("Duplicate/invalid lane")
                seen.add(lane)
                if any(x>0xffffffff or x<0 for x in values):raise ValueError("Invalid word")
                a,b,c,ah,bh,ch,sentinel,out=values
                low=out&0xffff;high=out>>16
                packed="packed low" in label or "packed high" in label
                result=(high if "packed high" in label else low) if packed else out
                preserved=(high==sentinel>>16 if "packed low" in label else low==sentinel&0xffff) if packed else ""
                yield "hpe",dict(common,lane=lane,a32=f"{a:08x}",b32=f"{b:08x}",c32=f"{c:08x}",a16=f"{ah:04x}",b16=f"{bh:04x}",c16=f"{ch:04x}",sentinel=f"{sentinel:08x}",output=f"{out:08x}",result_class=classify(result,16 if packed else 32),sentinel_preserved=preserved,wave=int(p[10]))
            if seen and len(seen)!=64:raise ValueError("Incomplete HPE test: "+label)
        else:
            for line in section.splitlines():
                if line.startswith("HPB_SAMPLE|"):
                    fields=dict(x.split("=",1) for x in line.split("|")[1:])
                    yield "hpb",dict(common,**fields)

def write(path,rows):
    if not rows:return
    with open(path,"w",newline="",encoding="utf-8") as f:
        w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)

def self_test():
    assert classify(0x80000000,32)=="-zero"
    assert classify(1,16)=="+subnormal"
    assert classify(0x7c00,16)=="+inf"
    assert classify(0x7e00,16)=="+nan"
    fixture="[GPU] test\n[TEST] HPE FP32_A Fma packed low default wave=32 | \n[AMD driver disassembly]\n v_fma_hp_mixlo_f16 v1, v2, v3 // 00000000: CC1E0001 00000000\n"
    fixture+="\n".join(f"HPE_ROW|{i}|0|0|0|0|0|0|b4003400|b4000001|32" for i in range(64))
    rows=list(parse(fixture))
    assert len(rows)==64 and rows[0][1]["sentinel_preserved"] and rows[0][1]["result_class"]=="+subnormal"
    assert "CC1E0001" in rows[0][1]["observed_fma_words"]
    assert list(parse("[GPU] test\n[TEST] HPB half candidate dependent groups=1 wave=32 | \nHPB_SAMPLE|index=0|wall_ms=1|checksum=1|changed_words=0|gpu_ns=100\n"))[0][0]=="hpb"
    print("HP extra export self-test PASS")

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument("capture",nargs="?");ap.add_argument("--prefix",default="hp-extra");ap.add_argument("--self-test",action="store_true");a=ap.parse_args()
    if a.self_test:self_test();return
    if not a.capture:ap.error("capture required")
    allrows={"hpe":[],"hpb":[]}
    if zipfile.is_zipfile(a.capture):
        with zipfile.ZipFile(a.capture) as z:
            names=[n for n in z.namelist() if "HP_especiais_RAW" in n or "HP_desempenho_RAW" in n]
            if not names:raise ValueError("No v0.14 HP extra RAW reports")
            for name in names:
                for kind,row in parse(z.read(name).decode("utf-8")):allrows[kind].append(row)
    else:
        with open(a.capture,encoding="utf-8") as f:
            for kind,row in parse(f.read()):allrows[kind].append(row)
    for kind,rows in allrows.items():
        write(a.prefix+"-"+kind+".csv",rows);print(kind,len(rows),"observational rows")
if __name__=="__main__":main()
