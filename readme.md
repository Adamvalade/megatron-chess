# Megatron Chess

Megatron is a C++ UCI chess engine with a small Node.js web frontend so visitors can play against it in the browser.

## Architecture

```
Browser (chessboard.js + chess.js)
    -> Express API (/api/move, /api/health)
        -> Megatron C++ executable (UCI: position / go depth N / bestmove)
```

The chess engine remains entirely in C++. The Node server is only a thin UCI bridge and static file host.

## Local development

### 1. Build the engine

```bash
make release
```

### 2. Install and run the web app

```bash
cd node_chess
npm install
ENGINE_PATH=../engine npm start
```

Open [http://localhost:3000](http://localhost:3000).

Optional environment variables:

- `PORT` — web server port (default `3000`)
- `ENGINE_PATH` — path to the compiled Megatron binary (default `../engine`)
- `ENGINE_DEPTH` — default search depth if the client omits one
- `ENGINE_TIMEOUT_MS` — max time to wait for `bestmove`

### 3. Engine-only checks

```bash
make release
./test_perft --unit
./test_uci.sh
make test_perft && ./test_perft
```

## API

### `GET /api/health`

```json
{ "ok": true, "engineFound": true, "engine": "engine" }
```

### `POST /api/move`

```json
{ "moves": ["e2e4", "e7e5"], "depth": 4 }
```

```json
{
  "bestmove": { "from": "g1", "to": "f3", "promotion": "" },
  "raw": "g1f3",
  "depth": 4
}
```

## Docker

```bash
docker build -t megatron-chess .
docker run --rm -p 3000:3000 megatron-chess
```

## Deployment notes

The previous Heroku setup assumed a Python engine (`Procfile` + Node server spawning `engine.py`). That cannot run the current C++ engine as-is.

Recommended options:

1. **Docker container** on Render, Fly.io, Railway, or similar using the included `Dockerfile`
2. Keep a Node host only if you also ship a prebuilt Linux `engine` binary and set `ENGINE_PATH`

Do not deploy a serverless Python-only host and expect the C++ executable to appear automatically.

## Project layout

- C++ engine sources at the repository root
- `node_chess/` — Express server and browser UI
- `Dockerfile` — builds the engine and serves the website
