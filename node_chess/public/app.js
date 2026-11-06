window.board = null;
window.game = null;
window.isThinking = false;

let engineSide = "black";
let searchDepth = 4;
let pendingPromotion = null;

const statusEl = document.getElementById("status");
const thinkingEl = document.getElementById("thinking");
const errorEl = document.getElementById("error");
const movesList = document.getElementById("moves-list");
const gameOverEl = document.getElementById("game-over");
const gameOverTitle = document.getElementById("game-over-title");
const gameOverMessage = document.getElementById("game-over-message");
const resetButton = document.getElementById("resetButton");
const gameOverReset = document.getElementById("gameOverReset");
const sideSelect = document.getElementById("sideSelect");
const depthSelect = document.getElementById("depthSelect");
const promotionModal = document.getElementById("promotion-modal");

function setThinking(isThinking) {
  window.isThinking = isThinking;
  thinkingEl.hidden = !isThinking;
  resetButton.disabled = isThinking;
  sideSelect.disabled = isThinking;
  depthSelect.disabled = isThinking;
}

function showError(message) {
  errorEl.hidden = !message;
  errorEl.textContent = message || "";
}

function hideGameOver() {
  gameOverEl.hidden = true;
}

function showGameOver(title, message) {
  gameOverTitle.textContent = title;
  gameOverMessage.textContent = message;
  gameOverEl.hidden = false;
}

function currentOrientation() {
  return engineSide === "white" ? "black" : "white";
}

function collectUciMoves() {
  return window.game.history({ verbose: true }).map((move) => {
    return move.from + move.to + (move.promotion || "");
  });
}

function renderMoves() {
  const history = window.game.history({ verbose: true });
  const fragment = document.createDocumentFragment();

  for (let index = 0; index < history.length; index += 2) {
    const row = document.createElement("li");
    row.className = "move-row";

    const moveNumber = document.createElement("div");
    moveNumber.className = "move-index";
    moveNumber.textContent = Math.floor(index / 2) + 1 + ".";
    row.appendChild(moveNumber);

    const whiteMove = document.createElement("div");
    whiteMove.textContent = history[index] ? history[index].san : "";
    row.appendChild(whiteMove);

    const blackMove = document.createElement("div");
    blackMove.textContent = history[index + 1] ? history[index + 1].san : "";
    row.appendChild(blackMove);

    fragment.appendChild(row);
  }

  movesList.innerHTML = "";
  movesList.appendChild(fragment);
  movesList.scrollTop = movesList.scrollHeight;
}

function updateStatus() {
  const turnName = window.game.turn() === "b" ? "Black" : "White";
  let status = turnName + " to move";

  if (window.game.in_checkmate()) {
    status = "Checkmate — " + turnName + " loses";
  } else if (window.game.in_stalemate()) {
    status = "Stalemate";
  } else if (window.game.in_draw()) {
    status = "Draw";
  } else if (window.game.in_check()) {
    status = turnName + " to move (check)";
  }

  statusEl.textContent = status;
  renderMoves();
}

function maybeShowGameOver() {
  if (window.game.in_checkmate()) {
    const losingSide = window.game.turn() === "w" ? "White" : "Black";
    const engineWon =
      (engineSide === "white" && losingSide === "Black") ||
      (engineSide === "black" && losingSide === "White");
    showGameOver(
      engineWon ? "Megatron wins" : "You win",
      losingSide + " is checkmated."
    );
    return true;
  }

  if (window.game.in_stalemate()) {
    showGameOver("Draw", "The game ended in stalemate.");
    return true;
  }

  if (window.game.in_draw()) {
    showGameOver("Draw", "The game ended in a draw.");
    return true;
  }

  return false;
}

function engineShouldMove() {
  if (window.game.game_over()) {
    return false;
  }
  const turn = window.game.turn();
  return (
    (engineSide === "white" && turn === "w") ||
    (engineSide === "black" && turn === "b")
  );
}

async function requestEngineMove() {
  if (window.isThinking || !engineShouldMove()) {
    return;
  }

  setThinking(true);
  showError("");

  try {
    const response = await fetch("/api/move", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        moves: collectUciMoves(),
        depth: searchDepth,
      }),
    });

    const data = await response.json();
    if (!response.ok) {
      throw new Error(data.error || "Engine request failed");
    }

    if (!data.bestmove) {
      updateStatus();
      maybeShowGameOver();
      return;
    }

    const played = window.game.move({
      from: data.bestmove.from,
      to: data.bestmove.to,
      promotion: data.bestmove.promotion || undefined,
    });

    if (!played) {
      throw new Error("Engine returned an illegal move");
    }

    window.board.position(window.game.fen());
    updateStatus();
    maybeShowGameOver();
  } catch (error) {
    showError(error.message || "Failed to contact Megatron");
  } finally {
    setThinking(false);
  }
}

function onDragStart(source, piece) {
  if (window.game.game_over() || window.isThinking) {
    return false;
  }
  if (
    (window.game.turn() === "w" && piece.search(/^b/) !== -1) ||
    (window.game.turn() === "b" && piece.search(/^w/) !== -1)
  ) {
    return false;
  }
  if (
    (engineSide === "white" && window.game.turn() === "w") ||
    (engineSide === "black" && window.game.turn() === "b")
  ) {
    return false;
  }
  return true;
}

function applyUserMove(source, target, promotionPiece) {
  const moveResult = window.game.move({
    from: source,
    to: target,
    promotion: promotionPiece || undefined,
  });

  if (!moveResult) {
    return "snapback";
  }

  window.board.position(window.game.fen());
  updateStatus();

  if (maybeShowGameOver()) {
    return;
  }

  requestEngineMove();
}

function choosePromotion(color) {
  return new Promise((resolve) => {
    pendingPromotion = resolve;
    promotionModal.hidden = false;
    promotionModal.innerHTML = "";
    ["q", "r", "n", "b"].forEach((piece) => {
      const image = document.createElement("img");
      image.className = "promo-icon";
      image.dataset.piece = piece;
      image.src = `chesspieces/pieces/${color}${piece.toUpperCase()}.png`;
      image.alt = piece;
      image.onclick = () => {
        promotionModal.hidden = true;
        const finish = pendingPromotion;
        pendingPromotion = null;
        finish(piece);
      };
      promotionModal.appendChild(image);
    });
  });
}

async function onDrop(source, target) {
  if (window.isThinking) {
    return "snapback";
  }

  const piece = window.game.get(source);
  if (!piece) {
    return "snapback";
  }

  const targetRank = target.charAt(1);
  const needsPromotion =
    piece.type === "p" && (targetRank === "8" || targetRank === "1");

  if (needsPromotion) {
    const probe = window.game.move({
      from: source,
      to: target,
      promotion: "q",
    });
    if (!probe) {
      return "snapback";
    }
    window.game.undo();
    const promotionPiece = await choosePromotion(
      piece.color === "w" ? "w" : "b"
    );
    applyUserMove(source, target, promotionPiece);
    return "snapback";
  }

  const result = applyUserMove(source, target, "");
  return result === "snapback" ? "snapback" : undefined;
}

function onSnapEnd() {
  window.board.position(window.game.fen());
}

function createBoard() {
  window.board = Chessboard("board", {
    draggable: true,
    position: "start",
    orientation: currentOrientation(),
    pieceTheme: "chesspieces/pieces/{piece}.png",
    onDrop,
    onDragStart,
    onSnapEnd,
  });
}

function resetGame() {
  hideGameOver();
  showError("");
  setThinking(false);
  window.game = new Chess();
  document.getElementById("board").innerHTML = "";
  createBoard();
  updateStatus();
  if (engineShouldMove()) {
    requestEngineMove();
  }
}

sideSelect.addEventListener("change", (event) => {
  engineSide = event.target.value;
  resetGame();
});

depthSelect.addEventListener("change", (event) => {
  searchDepth = Number(event.target.value) || 4;
});

resetButton.addEventListener("click", resetGame);
gameOverReset.addEventListener("click", resetGame);

document.addEventListener("DOMContentLoaded", () => {
  engineSide = sideSelect.value;
  searchDepth = Number(depthSelect.value) || 4;
  window.game = new Chess();
  createBoard();
  updateStatus();
});
