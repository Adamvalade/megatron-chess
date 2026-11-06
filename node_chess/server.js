const path = require("path");
const fs = require("fs");
const { spawn } = require("child_process");
const express = require("express");

const app = express();
const PORT = process.env.PORT || 3000;
const ENGINE_PATH =
  process.env.ENGINE_PATH || path.join(__dirname, "..", "engine");
const DEFAULT_DEPTH = Number(process.env.ENGINE_DEPTH || 4);
const MOVE_TIMEOUT_MS = Number(process.env.ENGINE_TIMEOUT_MS || 60000);

app.use(express.json({ limit: "32kb" }));
app.use(
  "/chesspieces",
  express.static(path.join(__dirname, "public/chesspieces"), {
    maxAge: "365d",
  })
);
app.use(express.static(path.join(__dirname, "public")));

const inFlight = new Set();

function sessionKey(req) {
  return (
    req.headers["x-session"] ||
    `${req.ip}|${req.headers["user-agent"] || "unknown"}`
  );
}

function resolveEnginePath() {
  if (fs.existsSync(ENGINE_PATH) && fs.statSync(ENGINE_PATH).isFile()) {
    return ENGINE_PATH;
  }
  const candidates = [
    path.join(__dirname, "..", "engine"),
    path.join(__dirname, "bin", "megatron"),
    path.join(__dirname, "megatron"),
  ];
  for (const candidate of candidates) {
    if (fs.existsSync(candidate) && fs.statSync(candidate).isFile()) {
      return candidate;
    }
  }
  return null;
}

function normalizeMoves(body) {
  if (!body) {
    return [];
  }
  if (Array.isArray(body.moves)) {
    return body.moves.map(String);
  }
  if (Array.isArray(body["moves[]"])) {
    return body["moves[]"].map(String);
  }
  if (typeof body.moves === "string" && body.moves.trim()) {
    return body.moves.trim().split(/\s+/);
  }
  return [];
}

function parseDepth(body) {
  const raw = Number(body && body.depth != null ? body.depth : DEFAULT_DEPTH);
  if (!Number.isFinite(raw)) {
    return DEFAULT_DEPTH;
  }
  return Math.max(1, Math.min(8, Math.floor(raw)));
}

function isValidUciMoveToken(token) {
  return /^[a-h][1-8][a-h][1-8][qrbn]?$/i.test(token);
}

function askEngine(moves, depth) {
  return new Promise((resolve, reject) => {
    const enginePath = resolveEnginePath();
    if (!enginePath) {
      reject(new Error("Megatron executable not found"));
      return;
    }

    const engine = spawn(enginePath, [], {
      stdio: ["pipe", "pipe", "pipe"],
    });

    let buffer = "";
    let settled = false;
    const timer = setTimeout(() => {
      finish(new Error("Engine timed out"));
    }, MOVE_TIMEOUT_MS);

    function finish(error, value) {
      if (settled) {
        return;
      }
      settled = true;
      clearTimeout(timer);
      try {
        engine.kill("SIGKILL");
      } catch (_) {}
      if (error) {
        reject(error);
      } else {
        resolve(value);
      }
    }

    engine.on("error", (error) => {
      finish(error);
    });

    engine.stderr.on("data", (chunk) => {
      process.stderr.write(chunk);
    });

    engine.stdout.on("data", (chunk) => {
      buffer += chunk.toString();
      let newline = buffer.indexOf("\n");
      while (newline !== -1) {
        const line = buffer.slice(0, newline).trim();
        buffer = buffer.slice(newline + 1);
        if (line.startsWith("bestmove")) {
          const parts = line.split(/\s+/);
          const moveText = parts[1] || "";
          if (!moveText || moveText === "0000" || moveText === "(none)") {
            finish(null, { bestmove: null, raw: moveText || "0000" });
            return;
          }
          finish(null, {
            bestmove: {
              from: moveText.slice(0, 2),
              to: moveText.slice(2, 4),
              promotion: moveText.slice(4) || "",
            },
            raw: moveText,
          });
          return;
        }
        newline = buffer.indexOf("\n");
      }
    });

    engine.on("close", (code) => {
      if (!settled) {
        finish(new Error(`Engine exited early with code ${code}`));
      }
    });

    let position = "position startpos";
    if (moves.length > 0) {
      position += " moves " + moves.join(" ");
    }

    engine.stdin.write("uci\n");
    engine.stdin.write("isready\n");
    engine.stdin.write("ucinewgame\n");
    engine.stdin.write(position + "\n");
    engine.stdin.write(`go depth ${depth}\n`);
  });
}

app.get("/api/health", (_req, res) => {
  const enginePath = resolveEnginePath();
  res.json({
    ok: true,
    engine: enginePath ? path.basename(enginePath) : null,
    engineFound: Boolean(enginePath),
  });
});

async function handleMoveRequest(req, res) {
  const key = sessionKey(req);
  if (inFlight.has(key)) {
    res.status(429).json({ error: "Another move is already in progress." });
    return;
  }
  inFlight.add(key);

  try {
    const moves = normalizeMoves(req.body);
    for (const move of moves) {
      if (!isValidUciMoveToken(move)) {
        res.status(400).json({ error: `Invalid move token: ${move}` });
        return;
      }
    }

    const depth = parseDepth(req.body);
    const result = await askEngine(moves, depth);
    res.json({
      bestmove: result.bestmove,
      raw: result.raw,
      depth,
    });
  } catch (error) {
    res.status(500).json({
      error: error && error.message ? error.message : "Engine failure",
    });
  } finally {
    inFlight.delete(key);
  }
}

app.post("/api/move", handleMoveRequest);
app.post("/move", handleMoveRequest);

app.listen(PORT, () => {
  const enginePath = resolveEnginePath();
  console.log(`Megatron web listening on http://localhost:${PORT}`);
  console.log(
    enginePath
      ? `Using engine executable: ${enginePath}`
      : "WARNING: engine executable not found. Build with `make release`."
  );
});
