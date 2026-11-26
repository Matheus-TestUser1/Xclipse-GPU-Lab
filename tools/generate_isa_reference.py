"""Build offline GFX1100 reference encodings. Never execute GPU code."""
import pathlib, re, subprocess, struct, sys
mc, seed, output = sys.argv[1:]
def assemble(text,wave):
    attr = '+wavefrontsize32,-wavefrontsize64' if wave=='32' else '-wavefrontsize32,+wavefrontsize64'
    p=subprocess.run([mc,'-triple=amdgcn-amd-amdhsa','-mcpu=gfx1100','-mattr='+attr,'-show-encoding'],input=text+'\n',text=True,capture_output=True)
    match=re.search(r'encoding:\s*\[([^\]]+)\]',p.stdout)
    if p.returncode or not match:
        return '','UNASSEMBLED'
    bs=bytes(int(x.strip(),16) for x in match[1].split(','))
    if len(bs)%4:return '', 'UNASSEMBLED'
    return ' '.join(f'{x:08X}' for x in struct.unpack('<'+'I'*(len(bs)//4),bs)), 'ASSEMBLED'
lines=['# LLVM '+subprocess.check_output([mc,'--version'],text=True).splitlines()[0].strip(), '# wave\tassembly\tMGFX2 observed words\tGFX1100 words\tstatus\tLLVM input']
def add(wave,asm,mgfx=''):
    # Translate only literal spelling; do not guess Samsung-only operand semantics.
    text=re.sub(r'\blit\((0x[0-9a-fA-F]+)\)',r'\1',asm)
    words,status=assemble(text,wave)
    lines.append('\t'.join((wave,asm,mgfx,words,status,text)))
for line in pathlib.Path(seed).read_text().splitlines():
    if not line or line.startswith('#'):continue
    wave,rest=line.split('|',1);asm,words=rest.rsplit('|',1)
    add(wave,asm,words)
for wave in ('32','64'):
    for asm in ('s_mov_b32 s0, s1','s_bfm_b64 s[0:1], 16, 0','v_pk_fma_f16 v0, v1, v2, v3','v_pk_add_f16 v0, v1, v2','v_pk_mul_f16 v0, v1, v2','v_dot2_f32_f16 v0, v1, v2, v3','v_dot4_i32_iu8 v0, v1, v2, v3'):
        add(wave,asm)
    dst='v[0:7]' if wave=='32' else 'v[0:3]'
    for op,src1,src2 in (
        ('v_wmma_f32_16x16x16_f16','v[8:15]','v[16:23]'),
        ('v_wmma_f32_16x16x16_bf16','v[8:15]','v[16:23]'),
        ('v_wmma_f16_16x16x16_f16','v[8:15]','v[16:23]'),
        ('v_wmma_bf16_16x16x16_bf16','v[8:15]','v[16:23]'),
        ('v_wmma_i32_16x16x16_iu8','v[8:11]','v[16:19]'),
        ('v_wmma_i32_16x16x16_iu4','v[8:9]','v[16:17]')):
        add(wave,f'{op} {dst}, {src1}, {src2}, {dst}')
path=pathlib.Path(output);path.parent.mkdir(parents=True,exist_ok=True);path.write_text('\n'.join(lines)+'\n')
if not any('\tASSEMBLED\t' in x for x in lines):raise SystemExit('LLVM reference generation produced no assembled instructions')
print('GFX1100 reference:',len(lines)-2,'samples;',sum('\tASSEMBLED\t' in x for x in lines),'assembled')
