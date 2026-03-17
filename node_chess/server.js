// server.js
const path = require("path");
const { spawn } = require("child_process");
const express = require("express");
const bodyParser = require("body-parser");


const app = express();

app.use('/chesspieces', express.static(path.join(__dirname, 'public/chesspieces'), {
  setHeaders: function (res, path) {
    if (path.endsWith('.png')) {
      res.setHeader('Cache-Control', 'public, max-age=31536000'); // 1 year
    }
  }
}));

const inFlight = new Set();

function getSessionKey(req) 
{
  return (req.headers['x-session'] || `${req.ip}|${req.headers['user-agent'] || ''}`);
}

app.use(express.static(path.join(__dirname, 'public'), {
  setHeaders: function (res, path) {
    if (path.endsWith('.png')) {
      res.setHeader('Cache-Control', 'public, max-age=31536000');
    }
  }
}));

app.use(bodyParser.urlencoded({ extended: true }));
app.use(bodyParser.json());

let engineOutputBuffer = "";
let readBuffer = []; 
let engineReady = false;


let waitingForBestmoveResolve = null;
function getBestmove(uciCommands) {
  return new Promise((resolve) => {
    readBuffer = [];

    waitingForBestmoveResolve = (line) => {
      resolve(line);
    };

    for (const cmd of uciCommands) {
      engine.stdin.write(cmd + "\n");
    }
  });
}


app.post("/move", async (req, res) => {

  const key = getSessionKey(req);               
  if (inFlight.has(key)) {                 
    return res.status(429).json({ error: "busy" }); 
  }                                                
  inFlight.add(key);                               

  let released = false;                             
  const release = () => {                          
    if (!released) { inFlight.delete(key); released = true; }
  };                                               

  console.log("[/move] Incoming request");
  const { spawn } = require("child_process");
  const enginePath = path.join(__dirname, "engine", "engine.py");
  const engine = spawn("python3", ["-m", "engine.engine"], { cwd: __dirname });

  const moves = (req.body && req.body["moves[]"]) ? req.body["moves[]"] : [];
  console.log("[/move] Moves received:", moves);

  let positionCmd = "position startpos";
  if (Array.isArray(moves) && moves.length > 0) {
    positionCmd += " moves " + moves.join(" ");
  }

  const goCmd = "go wtime 100000 btime 100000 winc 0 binc 0";
  console.log("[/move] Sending to engine:", positionCmd, goCmd);
  let buf = "";
  let responded = false;

    engine.stdout.on("data", (chunk) => {
    buf += chunk.toString();

    let nl;
    while ((nl = buf.indexOf("\n")) !== -1) {
      const line = buf.slice(0, nl).trim();
      buf = buf.slice(nl + 1); 

      if (!line) continue;
      console.log("[ENGINE STDOUT]", line);

      if (!responded && line.startsWith("bestmove")) {
        const parts = line.split(/\s+/);
        const mv = parts[1] || "";

        if (mv.length < 4 || mv === "0000" || mv === "(none)") {
          continue;
        }

        const bestmove = {
          from: mv.slice(0, 2),
          to: mv.slice(2, 4),
          promotion: mv.slice(4) || "",
        };

        responded = true;
        console.log("[/move] Responding with bestmove:", bestmove);
        res.json({ bestmove });
        try { engine.kill(); } catch {}
      }
    }
  });

  engine.stderr.on("data", (err) => {
    console.error("[ENGINE STDERR]", err.toString());
  });

  engine.on("close", (code) => {
    if (!responded) {
      console.error("[/move] Engine closed before bestmove. Code:", code);
      responded = true;
      res.status(500).json({ error: "Engine closed without bestmove." });
    }
    release();
  });

  engine.stdin.write("uci\n");
  engine.stdin.write("isready\n");
  engine.stdin.write(positionCmd + "\n");
  engine.stdin.write(goCmd + "\n");

  setTimeout(() => {
    if (!responded) {
      console.error("[/move] Timeout reached. Killing engine.");
      responded = true;
      engine.kill();
      res.status(500).json({ error: "Engine timed out." });
      release();
    }
  }, 25000);
});


const port = process.env.PORT || 3000;
app.listen(port, () => {
  console.log(`[SERVER] Listening on http://localhost:${port}`);
});
