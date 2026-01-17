#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GLSL_DIR="$SCRIPT_DIR/glsl"
SPIRV_DIR="$SCRIPT_DIR/spirv"

mkdir -p "$SPIRV_DIR"

for shader in "$GLSL_DIR"/*.vs.glsl; do
    name=$(basename "$shader" .vs.glsl)
    echo "Compiling $name.vs.glsl"
    glslangValidator -V -S vert "$shader" -o "$SPIRV_DIR/${name}.vs.spv"
done

for shader in "$GLSL_DIR"/*.fs.glsl; do
    name=$(basename "$shader" .fs.glsl)
    echo "Compiling $name.fs.glsl"
    glslangValidator -V -S frag "$shader" -o "$SPIRV_DIR/${name}.fs.spv"
done
