#!/usr/bin/env bash
set -euo pipefail

SHADERC=${SHADERC:-shaderc}
SRC_DIR=shaders/bgfx_src
OUT_ROOT=shaders/out
BGFX_INC=third_party/bgfx.cmake/bgfx/src

# profile_id : shaderc_platform : shaderc_profile
PROFILES=(
  "gl:linux:430"
  "gles:android:310_es"
  "webgl:asm.js:300_es"
  "vulkan:linux:spirv"
  "metal:osx:metal"
  "d3d:windows:s_5_0"
)

for entry in "${PROFILES[@]}"; do
    IFS=: read -r id platform profile <<< "$entry"
    out_dir="$OUT_ROOT/$id"
    mkdir -p "$out_dir"

    for src in "$SRC_DIR"/*.vert.sc "$SRC_DIR"/*.frag.sc; do
        [ -e "$src" ] || continue
        base=$(basename "$src" .sc)
        type=vertex
        [[ "$src" == *.frag.sc ]] && type=fragment

        out="$out_dir/$base.bin"
        echo "  $id: $base"
        "$SHADERC" -f "$src" -o "$out" \
            --platform "$platform" --profile "$profile" --type "$type" \
            -i "$BGFX_INC" || echo "    FAILED ($id / $base)"
    done
done
