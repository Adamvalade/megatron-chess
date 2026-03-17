# Megatron — Chess Engine

Megatron is a chess engine with a **Python backend** and a **Node.js/Express API**. It serves a small static client from `/public` and is deployed on **Heroku**.

---

## What’s here

```
.
├── Procfile               # Heroku entry (uses node server.js)
├── server.js              # Node/Express server (entrypoint)
├── public/                # Static client (board UI, assets)
│   ├── index.html
│   ├── app.js
│   ├── chessboard.js / chess.min.js / chessboard.css
│   └── chesspieces/pieces/*.png
├── engine.py              # Python engine entry (UCI-style wrapper)
├── uci.py                 # UCI helpers
├── libengine.so           # compiled engine library (used by Python)
├── requirements.txt       # Python deps
├── runtime.txt            # Heroku Python runtime (keep as-is)
├── package.json           # Node deps/scripts
└── README.md
```

---

## Quickstart

```bash
# 1) Install Node deps
npm ci

# 2) Python env + deps
python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt

# 3) Run the server (uses server.js)
npm start   # or: node server.js
```

The Node server spawns/communicates with the Python engine.

---

## Configuration

Create `.env` (or set env vars) if you need to override defaults:

```
PORT=3000
ENGINE_CMD="python engine.py"
```

Heroku config example:

```bash
heroku config:set NODE_ENV=production PORT=8080 ENGINE_CMD="python engine.py"
```

---

## API

**POST** `/api/move`

```json
{ "moves": ["e2e4", "e7e5"] }
```

Response:

```json
{ "bestmove": "g1f3" }
```

**GET** `/api/health`

```json
{ "ok": true }
```

---

## Deploy (Heroku)

```bash
git push heroku main        # or main:master depending on your remote
```

> The existing Procfile and buildpacks already work—don’t change them.

---

## Notes

* **Static UI** lives in `/public` and is served by `server.js`.
* **Python** engine runs via `ENGINE_CMD` (default `python engine.py`).
* **libengine.so** is included and used by the Python engine; keep it alongside `engine.py`.

---

## License

MIT © Adam Valade
