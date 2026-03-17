#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

# prefer venv python if present
if [[ -x ".venv/bin/python" ]]; then
  export PYTHON_BIN="$(pwd)/.venv/bin/python"
else
  export PYTHON_BIN="$(command -v python3)"
fi

echo "==> Using PYTHON_BIN=$PYTHON_BIN"
npm start
