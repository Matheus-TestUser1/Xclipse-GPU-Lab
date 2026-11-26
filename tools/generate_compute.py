import sys, subprocess, struct, pathlib, tempfile
glslc, src, out = sys.argv[1:]
with tempfile.TemporaryDirectory() as tmp:
    lines = ['#pragma once', '#include <cstdint>']
    for i in range(4):
        spv = pathlib.Path(tmp) / f'test{i}.spv'
        subprocess.run([glslc, '--target-env=vulkan1.2', f'-DTEST={i}', '-fshader-stage=compute', src, '-o', str(spv)], check=True)
        data=spv.read_bytes()
        words=struct.unpack('<'+'I'*(len(data)//4),data)
        lines.append(f'static constexpr uint32_t probe{i}[] = {{'+','.join(hex(w) for w in words)+'};')
    pathlib.Path(out).write_text('\n'.join(lines)+'\n')
