#!/usr/bin/env bash
set -u
SHADERC=${SHADERC:-shaderc}
BGFX_INC=third_party/bgfx.cmake/bgfx/src
VARYINGDEF=shaders/bgfx_src/varying.def.sc

# Format: outdir:platform:profile
# Add or remove entries to control which backends are built.
BACKENDS=(
  "gl:linux:430"
  "webgl:asm.js:300_es"
  "vulkan:linux:spirv"
  # "d3d:windows:s_5_0"   # needs Windows to compile
  # "metal:osx:metal"     # needs macOS to compile
)

mkdir -p shaders/out
for b in "${BACKENDS[@]}"; do
    IFS=: read -r outdir _ _ <<< "$b"
    mkdir -p "shaders/out/$outdir"
done

# Track which (shader, backend) pairs fail so we can report cleanly.
FAILS=0
OKS=0

for src in shaders/bgfx_src/*.vert.sc shaders/bgfx_src/*.frag.sc; do
    [ -e "$src" ] || continue
    base=$(basename "$src" .sc)
    if [[ "$src" == *.vert.sc ]]; then
        type=vertex
    else
        type=fragment
    fi

    for b in "${BACKENDS[@]}"; do
        IFS=: read -r outdir platform profile <<< "$b"
        out="shaders/out/$outdir/$base.bin"

        if $SHADERC --varyingdef "$VARYINGDEF" \
                    -f "$src" -o "$out" \
                    --platform "$platform" --profile "$profile" --type "$type" \
                    -i "$BGFX_INC" >/dev/null 2>&1; then
            echo "  ok: $outdir/$base.bin"
            OKS=$((OKS + 1))
        else
            echo "  FAIL: $outdir/$base.bin"
            FAILS=$((FAILS + 1))
        fi
    done
done

echo ""
echo "== $OKS ok, $FAILS failed =="

# Re-run failures with full output if any failed, for debugging.
if [ "$FAILS" -gt 0 ]; then
    echo ""
    echo "== Re-running failures verbosely =="
    for src in shaders/bgfx_src/*.vert.sc shaders/bgfx_src/*.frag.sc; do
        [ -e "$src" ] || continue
        base=$(basename "$src" .sc)
        if [[ "$src" == *.vert.sc ]]; then type=vertex; else type=fragment; fi
        for b in "${BACKENDS[@]}"; do
            IFS=: read -r outdir platform profile <<< "$b"
            out="shaders/out/$outdir/$base.bin"
            if [ ! -f "$out" ]; then
                echo ""
                echo "--- $outdir/$base.bin ---"
                $SHADERC --varyingdef "$VARYINGDEF" \
                         -f "$src" -o "$out" \
                         --platform "$platform" --profile "$profile" --type "$type" \
                         -i "$BGFX_INC" 2>&1 | tail -20
            fi
        done
    done
fi
