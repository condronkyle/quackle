#!/bin/bash
# Build Quackle engine as WebAssembly module
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
OUT_DIR="$SCRIPT_DIR/dist"

mkdir -p "$OUT_DIR"

# Core engine sources (all Qt-free)
ENGINE_SOURCES=(
    "$ROOT_DIR/alphabetparameters.cpp"
    "$ROOT_DIR/bag.cpp"
    "$ROOT_DIR/board.cpp"
    "$ROOT_DIR/boardparameters.cpp"
    "$ROOT_DIR/bogowinplayer.cpp"
    "$ROOT_DIR/catchall.cpp"
    "$ROOT_DIR/clock.cpp"
    "$ROOT_DIR/computerplayer.cpp"
    "$ROOT_DIR/computerplayercollection.cpp"
    "$ROOT_DIR/cputopology.cpp"
    "$ROOT_DIR/datamanager.cpp"
    "$ROOT_DIR/endgame.cpp"
    "$ROOT_DIR/endgameplayer.cpp"
    "$ROOT_DIR/enumerator.cpp"
    "$ROOT_DIR/evaluator.cpp"
    "$ROOT_DIR/game.cpp"
    "$ROOT_DIR/gameparameters.cpp"
    "$ROOT_DIR/generator.cpp"
    "$ROOT_DIR/lexiconparameters.cpp"
    "$ROOT_DIR/move.cpp"
    "$ROOT_DIR/player.cpp"
    "$ROOT_DIR/playerlist.cpp"
    "$ROOT_DIR/preendgame.cpp"
    "$ROOT_DIR/rack.cpp"
    "$ROOT_DIR/reporter.cpp"
    "$ROOT_DIR/resolvent.cpp"
    "$ROOT_DIR/sim.cpp"
    "$ROOT_DIR/strategyparameters.cpp"
    "$ROOT_DIR/test/crossplayboards.cpp"
)

# WASM bindings
WASM_SOURCES=(
    "$SCRIPT_DIR/quackle_wasm.cpp"
)

# Data files to embed in the WASM binary
# Using --preload-file creates a separate .data file for async loading
DATA_DIR="$ROOT_DIR/data"
GADDAG_FILE="$DATA_DIR/lexica/nwl23.gaddag"
GADDAG_PRELOAD=()
if [[ -f "$GADDAG_FILE" ]]; then
    GADDAG_PRELOAD=(--preload-file "$GADDAG_FILE@/data/lexica/nwl23.gaddag")
else
    echo "Warning: nwl23.gaddag is absent; the build will use slower DAWG move generation."
fi

echo "=== Building Quackle WASM ==="
echo "Root: $ROOT_DIR"
echo "Output: $OUT_DIR"

cd "$ROOT_DIR"

em++ \
    "${ENGINE_SOURCES[@]}" \
    "${WASM_SOURCES[@]}" \
    -I"$ROOT_DIR" \
    -I"$ROOT_DIR/test" \
    -std=c++17 \
    -O2 \
    -ffile-prefix-map="$ROOT_DIR"=/src/quackle \
    -s WASM=1 \
    -s MODULARIZE=1 \
    -s EXPORT_NAME="QuackleModule" \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s INITIAL_MEMORY=67108864 \
    -s MAXIMUM_MEMORY=268435456 \
    -s STACK_SIZE=1048576 \
    -s NO_EXIT_RUNTIME=1 \
    -s ENVIRONMENT=web,worker \
    --bind \
    --preload-file "$DATA_DIR/alphabets/crossplay.quackle_alphabet@/data/alphabets/crossplay.quackle_alphabet" \
    --preload-file "$DATA_DIR/lexica/nwl23.dawg@/data/lexica/nwl23.dawg" \
    "${GADDAG_PRELOAD[@]}" \
    --preload-file "$DATA_DIR/strategy/default_english/@/data/strategy/default_english/" \
    --preload-file "$DATA_DIR/strategy/default/bogowin@/data/strategy/default/bogowin" \
    -o "wasm/dist/quackle.js"

echo ""
echo "=== Build complete ==="
ls -lh "$OUT_DIR"/quackle.*
echo ""
echo "Files:"
echo "  quackle.js    - JS loader/glue code"
echo "  quackle.wasm  - WebAssembly binary"
echo "  quackle.data  - Embedded data files (dictionaries, strategy)"
