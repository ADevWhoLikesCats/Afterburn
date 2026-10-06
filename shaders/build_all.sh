#!/usr/bin/env bash
set -u
SHADERC=${SHADERC:-shaderc}
BGFX_INC=third_party/bgfx.cmake/bgfx/src

mkdir -p shaders/out/gl shaders/out/webgl

for src in shaders/bgfx_src/*.vert.sc shaders/bgfx_src/*.frag.sc; do
    [ -e "$src" ] || continue
    base=$(basename "$src" .sc)
    type=fragment
    [[ "$src" == *.vert.sc ]] && type=vertex

    for plat in "linux:430:gl" "asm.js:300_es:webgl"; do
        IFS=: read -r platform profile outdir <<< "$plat"
        out="shaders/out/$outdir/$base.bin"
        if $SHADERC -f "$src" -o "$out" \
            --platform $platform --profile $profile --type $type \
            -i "$BGFX_INC" 2>&1; then
            echo "  ok: $outdir/$base.bin"
        else
            echo "  FAIL: $outdir/$base.bin"
        fi
    done
done
