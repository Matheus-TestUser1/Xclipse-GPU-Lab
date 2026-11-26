import sys, subprocess, struct, pathlib, tempfile, shutil

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

glslc, src, out = sys.argv[1:]
with tempfile.TemporaryDirectory() as tmp:
    lines = ['#pragma once', '#include <cstdint>']
    for i in (0,1,2,3,5,4,6,7,8,9):
        spv = pathlib.Path(tmp) / f'test{i}.spv'
        if i in (6,7):
            assembler, validator = shutil.which('spirv-as'), shutil.which('spirv-val')
            asm=pathlib.Path(tmp)/f'test{i}.spvasm'
            asm.write_text(dot_asm(8 if i==6 else 16))
            ok=bool(assembler and validator)
            if ok:
                ok=subprocess.run([assembler,'--target-env','vulkan1.2',str(asm),'-o',str(spv)]).returncode==0
            if ok:
                ok=subprocess.run([validator,'--target-env','vulkan1.2',str(spv)]).returncode==0
        else:
            ok=subprocess.run([glslc, '--target-env=vulkan1.2', f'-DTEST={i}', '-fshader-stage=compute', src, '-o', str(spv)]).returncode==0
        if not ok:
            if i not in (4,6,7,8,9):
                raise SystemExit(1)
            print(f'Optional shader {i} unavailable at build time')
            lines.append(f'static constexpr bool has_probe{i} = false;')
            lines.append(f'static constexpr uint32_t probe{i}[] = {{0}};')
            continue
        if i in (4,6,7,8,9): lines.append(f'static constexpr bool has_probe{i} = true;')
        data=spv.read_bytes()
        words=struct.unpack('<'+'I'*(len(data)//4),data)
        lines.append(f'static constexpr uint32_t probe{i}[] = {{'+','.join(hex(w) for w in words)+'};')
    pathlib.Path(out).write_text('\n'.join(lines)+'\n')
