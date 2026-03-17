#!/usr/bin/env bash
set -euo pipefail

# repo root = this script's parent dir
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

PY=python3

echo "==> Creating venv (.venv)"
$PY -m venv .venv
source .venv/bin/activate

echo "==> Upgrading pip/setuptools/wheel"
python -m pip install --upgrade pip setuptools wheel

echo "==> Installing Python deps"
# add your actual Python deps here too (Flask, etc.) if engine/engine.py needs them
pip install pybind11

echo "==> Installing Node deps"
npm ci

echo "==> Done. To activate later: 'source .venv/bin/activate'"

