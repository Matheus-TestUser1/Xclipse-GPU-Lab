import sys, subprocess, struct, pathlib, tempfile, shutil, re, json

def dot_asm(bits):
    return f"""OpCapability Shader
OpCapability Int{bits}
OpCapability DotProductKHR
OpCapability DotProductInputAllKHR
OpExtension "SPV_KHR_integer_dot_product"
OpMemoryModel Logical GLSL450
OpEntryPoint GLCompute %main "main" %gid %buf
OpExecutionMode %main LocalSize 64 1 1
OpDecorate %gid BuiltIn LocalInvocationId
OpDecorate %arr ArrayStride 4
OpMemberDecorate %block 0 Offset 0
OpDecorate %block Block
OpDecorate %buf DescriptorSet 0
OpDecorate %buf Binding 0
%void = OpTypeVoid
%ft = OpTypeFunction %void
%uint = OpTypeInt 32 0
%int = OpTypeInt 32 1
%narrow = OpTypeInt {bits} 1
%vec = OpTypeVector %narrow 2
%u3 = OpTypeVector %uint 3
%pin = OpTypePointer Input %u3
%gid = OpVariable %pin Input
%arr = OpTypeRuntimeArray %uint
%block = OpTypeStruct %arr
%pblock = OpTypePointer StorageBuffer %block
%buf = OpVariable %pblock StorageBuffer
%ptr = OpTypePointer StorageBuffer %uint
%zero = OpConstant %uint 0
%sixtyfour = OpConstant %uint 64
%one = OpConstant %narrow 1
%two = OpConstant %narrow 2
%three = OpConstant %narrow 3
%main = OpFunction %void None %ft
%entry = OpLabel
%gidv = OpLoad %u3 %gid
%i = OpCompositeExtract %uint %gidv 0
%idx = OpIAdd %uint %i %sixtyfour
%inputptr = OpAccessChain %ptr %buf %zero %idx
%input = OpLoad %uint %inputptr
%si = OpBitcast %int %input
%a = OpSConvert %narrow %si
%ap = OpIAdd %narrow %a %one
%x = OpCompositeConstruct %vec %a %ap
%y = OpCompositeConstruct %vec %two %three
%dot = OpSDotKHR %int %x %y
%answer = OpBitcast %uint %dot
%outptr = OpAccessChain %ptr %buf %zero %i
OpStore %outptr %answer
OpReturn
OpFunctionEnd
"""


def matrix_asm(kind):
    floating=kind==12
    signed=kind==13
    cap='Float16' if floating else 'Int8'
    types='%narrow = OpTypeFloat 16\n%out = OpTypeFloat 32' if floating else ('%narrow = OpTypeInt 8 1\n%out = OpTypeInt 32 1' if signed else '%narrow = OpTypeInt 8 0')
    out='%out' if floating or signed else '%uint'
    convert='OpFConvert' if floating else ('OpSConvert' if signed else 'OpUConvert')
    signed_flags=' MatrixASignedComponentsKHR|MatrixBSignedComponentsKHR|MatrixCSignedComponentsKHR|MatrixResultSignedComponentsKHR' if signed else ''
    return f"""OpCapability Shader
OpCapability GroupNonUniform
OpCapability {cap}
OpCapability VulkanMemoryModel
OpCapability CooperativeMatrixKHR
OpExtension "SPV_KHR_cooperative_matrix"
OpMemoryModel Logical Vulkan
OpEntryPoint GLCompute %main "main" %sub %buf
OpExecutionMode %main LocalSize 64 1 1
OpDecorate %sub BuiltIn SubgroupId
OpDecorate %arr ArrayStride 4
OpMemberDecorate %block 0 Offset 0
OpDecorate %block Block
OpDecorate %buf DescriptorSet 0
OpDecorate %buf Binding 0
%void = OpTypeVoid
%ft = OpTypeFunction %void
%uint = OpTypeInt 32 0
{types}
%pin = OpTypePointer Input %uint
%sub = OpVariable %pin Input
%arr = OpTypeRuntimeArray {out}
%block = OpTypeStruct %arr
%pblock = OpTypePointer StorageBuffer %block
%buf = OpVariable %pblock StorageBuffer
%ptr = OpTypePointer StorageBuffer {out}
%zero = OpConstant %uint 0
%one = OpConstant %uint 1
%two = OpConstant %uint 2
%scope = OpConstant %uint 3
%dim = OpConstant %uint 16
%tile = OpConstant %uint 256
%inA = OpConstant %uint 512
%inB = OpConstant %uint 513
%c0 = OpConstant {out} 0
%matA = OpTypeCooperativeMatrixKHR %narrow %scope %dim %dim %zero
%matB = OpTypeCooperativeMatrixKHR %narrow %scope %dim %dim %one
%matC = OpTypeCooperativeMatrixKHR {out} %scope %dim %dim %two
%main = OpFunction %void None %ft
%entry = OpLabel
%pa = OpAccessChain %ptr %buf %zero %inA
%pb = OpAccessChain %ptr %buf %zero %inB
%av = OpLoad {out} %pa
%bv = OpLoad {out} %pb
%a = {convert} %narrow %av
%b = {convert} %narrow %bv
%ma = OpCompositeConstruct %matA %a
%mb = OpCompositeConstruct %matB %b
%mc = OpCompositeConstruct %matC %c0
%result = OpCooperativeMatrixMulAddKHR %matC %ma %mb %mc{signed_flags}
%sg = OpLoad %uint %sub
%off = OpIMul %uint %sg %tile
%dest = OpAccessChain %ptr %buf %zero %off
OpCooperativeMatrixStoreKHR %dest %result %zero %dim
OpReturn
OpFunctionEnd
"""

glslc, src, out = sys.argv[1:]
with tempfile.TemporaryDirectory() as tmp:
    lines = ['#pragma once', '#include <cstdint>']
    compiler=subprocess.check_output([glslc,'--version'],text=True).strip()
    lines.append('static constexpr const char* fma_compiler_metadata = '+json.dumps(compiler)+';')
    for i in (0,1,2,3,5,4,6,7,8,9,10,11,12,13,14,*range(15,69)):
        spv = pathlib.Path(tmp) / f'test{i}.spv'
        if i in (6,7,12,13,14):
            assembler, validator = shutil.which('spirv-as'), shutil.which('spirv-val')
            asm=pathlib.Path(tmp)/f'test{i}.spvasm'
            asm.write_text(dot_asm(8 if i==6 else 16) if i in (6,7) else matrix_asm(i))
            ok=bool(assembler and validator)
            if ok:
                ok=subprocess.run([assembler,'--target-env','vulkan1.2',str(asm),'-o',str(spv)]).returncode==0
            if ok:
                ok=subprocess.run([validator,'--target-env','vulkan1.2',str(spv)]).returncode==0
        else:
            ok=subprocess.run([glslc, '--target-env=vulkan1.2', f'-DTEST={i}', '-fshader-stage=compute', src, '-o', str(spv)]).returncode==0
        if ok and i>=15:
            # Require real F16 arithmetic, validate every new shader, and retain a
            # byte-for-byte fingerprint of the SPIR-V that will reach the driver.
            disassembler, assembler, validator = (shutil.which(x) for x in ('spirv-dis','spirv-as','spirv-val'))
            if not all((disassembler,assembler,validator)):
                raise SystemExit('FMA research requires spirv-dis, spirv-as and spirv-val')
            text=subprocess.check_output([disassembler,str(spv)],text=True)
            if not re.search(r'OpTypeFloat\s+16',text):
                raise SystemExit('FMA probe lost its Float16 type')
            split_control=((i-15)%3==2) if i<24 else ((i-24)%6==4) if i<42 else ((i-42)%9==7)
            fma_builtin=(i<24 and not split_control) or (24<=i<42 and (i-24)%6 in (2,3,5)) or (i>=42 and (i-42)%9 in (0,1,2,3,6,8))
            if split_control:
                if 'NoContraction' not in text or 'OpFMul' not in text or 'OpFAdd' not in text:
                    raise SystemExit('FMA split control lost NoContraction/mul/add')
            elif fma_builtin and not re.search(r'OpExtInst .* Fma ',text):
                raise SystemExit('FMA probe lost GLSL.std.450 Fma')
            if i>=24:
                half_type=re.search(r'(%\w+)\s*=\s*OpTypeFloat\s+16',text).group(1)
                if not re.search(r'OpFConvert\s+'+re.escape(half_type)+r'\s',text):
                    raise SystemExit('HP probe lost its FP32-to-FP16 conversion')
            if ((24<=i<42 and (i-24)%6 in (0,1)) or (i>=42 and (i-42)%9 in (4,5))) and ('OpFMul' not in text or 'OpFAdd' not in text):
                raise SystemExit('HP mul/add reproducer lost arithmetic')
            mode=(i-15)//3 if i<24 else (i-24)//6 if i<42 else (i-42)//9
            if i>=42 and (i-42)%9 in (1,5) and 'PushConstant' not in text:
                raise SystemExit('HP uniform-source probe lost push constants')
            if mode:
                rounding='RoundingModeRTE' if mode==1 else 'RoundingModeRTZ'
                entry=re.search(r'OpEntryPoint GLCompute (%\w+)',text).group(1)
                text=text.replace('OpCapability Shader','OpCapability Shader\n               OpCapability '+rounding,1)
                modes='OpExecutionMode '+entry+' '+rounding+' 16\n               '
                if i>=24:
                    text=text.replace('OpCapability Shader','OpCapability Shader\n               OpCapability RoundingModeRTE',1) if mode==2 else text
                    modes+='OpExecutionMode '+entry+' RoundingModeRTE 32\n               '
                text=text.replace('OpExecutionMode '+entry+' LocalSize', modes+'OpExecutionMode '+entry+' LocalSize',1)
            asm=pathlib.Path(tmp)/f'fma{i}.spvasm';asm.write_text(text)
            subprocess.run([assembler,'--target-env','vulkan1.2',str(asm),'-o',str(spv)],check=True)
            subprocess.run([validator,'--target-env','vulkan1.2',str(spv)],check=True)
        if not ok:
            if i>=15:
                raise SystemExit(f'Required FMA research shader {i} failed to compile')
            if i not in (4,6,7,8,9,10,11,12,13,14):
                raise SystemExit(1)
            print(f'Optional shader {i} unavailable at build time')
            lines.append(f'static constexpr bool has_probe{i} = false;')
            lines.append(f'static constexpr uint32_t probe{i}[] = {{0}};')
            continue
        if i in (4,6,7,8,9,10,11,12,13,14): lines.append(f'static constexpr bool has_probe{i} = true;')
        data=spv.read_bytes()
        words=struct.unpack('<'+'I'*(len(data)//4),data)
        lines.append(f'static constexpr uint32_t probe{i}[] = {{'+','.join(hex(w) for w in words)+'};')
    pathlib.Path(out).write_text('\n'.join(lines)+'\n')
