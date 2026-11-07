#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

echo "==> Compiling Megatron C++ engine for Linux"
make clean || true
make release

if [[ ! -x "$ROOT/engine" ]]; then
  echo "ERROR: engine binary was not produced" >&2
  exit 1
fi

echo "==> Installing Node dependencies for node_chess"
npm --prefix node_chess ci --omit=dev

echo "==> Heroku build complete"
ls -la "$ROOT/engine"
