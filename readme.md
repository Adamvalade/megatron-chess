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
- `ENGINE_MAX_DEPTH` — maximum allowed search depth (default `8`)
- `ENGINE_TIMEOUT_MS` — base max wait for `bestmove` (default `600000`; also scales with depth)

### 3. Engine-only checks

```bash
make release
./test_perft --unit
./test_uci.sh
make test_perft && ./test_perft
```

## Using Megatron in a chess GUI

Megatron speaks the standard UCI protocol. After building with `make release`, point a UCI-compatible GUI (such as Arena, Cute Chess, or Nibbler) at the compiled `engine` binary.

Example session:

```text
uci
isready
ucinewgame
position startpos moves e2e4 e7e5
go depth 4
```

The engine replies with `bestmove …`.

## Publishing downloadable binaries

Local `make release` currently produces a machine-specific binary (for example macOS arm64). That file is not a portable Windows/Linux download.

To publish real downloads:

1. Build release binaries on each target platform, or cross-compile deliberately for:
   - Linux x86_64
   - macOS arm64 / x86_64
   - Windows x86_64
2. Name assets clearly, e.g. `megatron-linux-x86_64`, `megatron-macos-arm64`, `megatron-windows-x86_64.exe`
3. Create a GitHub Release on `Adamvalade/megatron-chess` and upload those assets
4. Update the website download link to the Release page or specific asset URLs

Until those assets exist, the site links to the GitHub repository for source builds instead of implying a universal executable download.

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

Production builds the Linux C++ engine during deploy, then starts the Node server.

### Heroku (buildpacks)

Root `Procfile` runs `node node_chess/server.js`. `heroku-postbuild` compiles Megatron with `make release` and installs `node_chess` dependencies. Recommended buildpacks (in order):

1. `heroku-community/apt` (provides `g++` / `make` via `Aptfile`)
2. `heroku/nodejs`

Set `ENGINE_PATH` to the compiled binary at the app root if needed (default search already finds `../engine` from `node_chess/server.js`).

### Heroku (container)

```bash
heroku stack:set container -a <app-name>
heroku container:push web -a <app-name>
heroku container:release web -a <app-name>
```

The included `Dockerfile` builds the engine and serves the website. The process listens on Heroku’s `PORT`.

### Other hosts

```bash
docker build -t megatron-chess .
docker run --rm -p 3000:3000 -e PORT=3000 megatron-chess
```

## Project layout

- C++ engine sources at the repository root
- `node_chess/` — Express server and browser UI
- `Dockerfile` — builds the engine and serves the website
