import sys, subprocess, struct, pathlib, tempfile
glslc, src, out = sys.argv[1:]
with tempfile.TemporaryDirectory() as tmp:
    lines = ['#pragma once', '#include <cstdint>']
    for i in (0,1,2,3,5,4):
        spv = pathlib.Path(tmp) / f'test{i}.spv'
        result = subprocess.run([glslc, '--target-env=vulkan1.2', f'-DTEST={i}', '-fshader-stage=compute', src, '-o', str(spv)])
        if result.returncode:
            if i != 4:
                raise SystemExit(result.returncode)
            print('Optional KHR cooperative matrix shader not supported by this compiler')
            lines.append('static constexpr bool has_probe4 = false;')
            lines.append('static constexpr uint32_t probe4[] = {0};')
            continue
        if i == 4: lines.append('static constexpr bool has_probe4 = true;')
        data=spv.read_bytes()
        words=struct.unpack('<'+'I'*(len(data)//4),data)
        lines.append(f'static constexpr uint32_t probe{i}[] = {{'+','.join(hex(w) for w in words)+'};')
    pathlib.Path(out).write_text('\n'.join(lines)+'\n')
