#!/usr/bin/env bash
set -euo pipefail

# ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# cd "$ROOT"

# # Use venv python if available
# if [[ -x ".venv/bin/python" ]]; then
#   PY=".venv/bin/python"
# else
#   PY="python3"
# fi

# # Where your C++ sources live:
# SRC_DIR="move_generator"
# OUT_MOD="megatron_core"  # Python import name: import megatron_core

# # Extra include dirs (adjust if you have headers)
# EXTRA_INCLUDES="-I ${SRC_DIR} -I ${SRC_DIR}/include"

# echo "==> Building pybind11 module '${OUT_MOD}' from ${SRC_DIR}/bindings.cpp ..."
# c++ -O3 -Wall -shared -std=c++20 -fPIC \
#   $($PY -m pybind11 --includes) \
#   ${EXTRA_INCLUDES} \
#   ${SRC_DIR}/bindings.cpp ${SRC_DIR}/board.cpp \
#   -o ${OUT_MOD}$($PY -c "import sysconfig; print(sysconfig.get_config_var('EXT_SUFFIX'))")

# echo "==> Build complete."
# echo "Test with: ${PY} -c 'import ${OUT_MOD} as m; print(\"OK\", dir(m))'"


cd bin
make all

