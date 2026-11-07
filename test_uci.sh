#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
ENGINE="${ENGINE:-${ROOT}/engine}"
PASS=0
FAIL=0

if [[ ! -x "$ENGINE" ]]; then
  echo "missing executable: $ENGINE (run make release first)"
  exit 1
fi

run_case() {
  local name="$1"
  local input="$2"
  shift 2
  local output
  output="$(printf '%s\n' "$input" | "$ENGINE" 2>/dev/null || true)"
  local pattern
  for pattern in "$@"; do
    if ! echo "$output" | grep -Eq -- "$pattern"; then
      echo "  FAIL  $name"
      echo "    missing /$pattern/"
      echo "    got:"
      echo "$output" | sed 's/^/      /'
      FAIL=$((FAIL + 1))
      return
    fi
  done
  echo "  PASS  $name"
  PASS=$((PASS + 1))
}

assert_legal_bestmove() {
  local name="$1"
  local input="$2"
  local output move
  output="$(printf '%s\n' "$input" | "$ENGINE" 2>/dev/null || true)"
  move="$(echo "$output" | awk '/^bestmove /{print $2; exit}')"
  if [[ -z "$move" ]]; then
    echo "  FAIL  $name (no bestmove)"
    FAIL=$((FAIL + 1))
    return
  fi
  if [[ "$move" == "0000" ]]; then
    echo "  FAIL  $name (got 0000)"
    FAIL=$((FAIL + 1))
    return
  fi
  if [[ "$move" =~ ^[a-h][1-8][a-h][1-8][qrbn]?$ ]]; then
    echo "  PASS  $name ($move)"
    PASS=$((PASS + 1))
  else
    echo "  FAIL  $name (bad move text: $move)"
    FAIL=$((FAIL + 1))
  fi
}

echo "UCI stdin/stdout tests:"

run_case "uci initialization" \
  $'uci\nquit' \
  'id name Megatron Chess' \
  'id author Adam Valade' \
  'uciok'

run_case "isready" \
  $'uci\nisready\nquit' \
  'readyok'

assert_legal_bestmove "startpos initialization + go" \
  $'uci\nucinewgame\nposition startpos\ngo depth 1\nquit'

assert_legal_bestmove "fen position initialization" \
  $'position fen rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1\ngo depth 1\nquit'

assert_legal_bestmove "apply uci move sequence" \
  $'position startpos moves e2e4 e7e5 g1f3\ngo depth 1\nquit'

assert_legal_bestmove "castling move accepted" \
  $'position fen r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1 moves e1g1\ngo depth 1\nquit'

assert_legal_bestmove "promotion move accepted" \
  $'position fen 8/P7/8/8/8/8/8/4K2k w - - 0 1 moves a7a8q\ngo depth 1\nquit'

assert_legal_bestmove "en passant move accepted" \
  $'position fen rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3 moves e5f6\ngo depth 1\nquit'

run_case "go depth returns uci move" \
  $'position startpos\ngo depth 2\nquit' \
  '^bestmove [a-h][1-8][a-h][1-8]'

run_case "stalemate returns bestmove 0000" \
  $'position fen 7k/5Q2/6K1/8/8/8/8/8 b - - 0 1\ngo depth 1\nquit' \
  '^bestmove 0000$'

run_case "checkmate returns bestmove 0000" \
  $'position fen rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 0 3\ngo depth 1\nquit' \
  '^bestmove 0000$'

run_case "invalid command does not crash" \
  $'uci\nthis is not a command\nisready\nquit' \
  'readyok'

run_case "illegal move is rejected safely" \
  $'position startpos moves e2e5\nisready\nquit' \
  'readyok'

run_case "quit terminates cleanly" \
  $'uci\nquit' \
  'uciok'

echo "UCI tests: ${PASS} passed, ${FAIL} failed"
if [[ "$FAIL" -ne 0 ]]; then
  exit 1
fi
exit 0
