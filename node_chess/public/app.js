window.board = null;
window.game = null;
window.isThinking = false;

const MIN_DEPTH = 1;
const MAX_DEPTH = 8;
const DEFAULT_DEPTH = 4;
const MOVE_ANIMATION_MS = 250;
const WIN_CELEBRATION_DELAY_MS = 1000;

let engineSide = "black";
let searchDepth = DEFAULT_DEPTH;
let pendingPromotion = null;
let moveRequestId = 0;
let activeMoveController = null;
let pendingAnimationResolve = null;
let celebrationToken = 0;
let celebrationTimer = null;
let selectedSquare = null;
let ignoreBoardClickUntil = 0;
let afterSnapHandler = null;

const statusEl = document.getElementById("status");
const thinkingEl = document.getElementById("thinking");
const thinkingDepthEl = document.getElementById("thinkingDepth");
const errorEl = document.getElementById("error");
const movesList = document.getElementById("moves-list");
const gameOverEl = document.getElementById("game-over");
const gameOverTitle = document.getElementById("game-over-title");
const gameOverMessage = document.getElementById("game-over-message");
const resultVideo = document.getElementById("result-highlight-video");
const megatronWinsEl = document.getElementById("megatron-wins");
const winVideo = document.getElementById("win-highlight-video");
const resetButton = document.getElementById("resetButton");
const gameOverReset = document.getElementById("gameOverReset");
const dismissWins = document.getElementById("dismissWins");
const winsPlayAgain = document.getElementById("winsPlayAgain");
const sideSelect = document.getElementById("sideSelect");
const depthSlider = document.getElementById("depthSlider");
const depthValue = document.getElementById("depthValue");
const depthRecommended = document.getElementById("depthRecommended");
const promotionModal = document.getElementById("promotion-modal");
const boardEl = document.getElementById("board");
const aboutButton = document.getElementById("aboutButton");
const aboutModal = document.getElementById("about-modal");
const aboutClose = document.getElementById("aboutClose");
const aboutCard = aboutModal.querySelector(".about-modal-card");

let aboutOpen = false;
let aboutLastFocus = null;

function prefersReducedMotion() {
  return (
    window.matchMedia &&
    window.matchMedia("(prefers-reduced-motion: reduce)").matches
  );
}

function clampDepth(value) {
  const depth = Number(value);
  if (!Number.isFinite(depth)) {
    return DEFAULT_DEPTH;
  }
  return Math.max(MIN_DEPTH, Math.min(MAX_DEPTH, Math.floor(depth)));
}

function syncDepthDisplay(depth) {
  searchDepth = clampDepth(depth);
  depthSlider.value = String(searchDepth);
  depthSlider.setAttribute("aria-valuenow", String(searchDepth));
  depthValue.textContent = String(searchDepth);
  thinkingDepthEl.textContent = String(searchDepth);
  depthRecommended.hidden = searchDepth !== DEFAULT_DEPTH;
}

function cancelActiveMoveRequest() {
  moveRequestId += 1;
  if (activeMoveController) {
    activeMoveController.abort();
    activeMoveController = null;
  }
}

function cancelCelebration() {
  celebrationToken += 1;
  if (celebrationTimer !== null) {
    clearTimeout(celebrationTimer);
    celebrationTimer = null;
  }
}

function clearSquareSelection() {
  const selected = boardEl.querySelectorAll(".is-selected");
  for (let i = 0; i < selected.length; i += 1) {
    selected[i].classList.remove("is-selected");
  }
  selectedSquare = null;
}

function setThinking(isThinking) {
  window.isThinking = isThinking;
  thinkingEl.classList.toggle("is-active", isThinking);
  thinkingEl.setAttribute("aria-hidden", isThinking ? "false" : "true");
  sideSelect.disabled = isThinking;
  depthSlider.disabled = isThinking;
  if (isThinking) {
    thinkingDepthEl.textContent = String(searchDepth);
  }
}

function showError(message) {
  const hasError = Boolean(message);
  errorEl.classList.toggle("is-active", hasError);
  errorEl.setAttribute("aria-hidden", hasError ? "false" : "true");
  errorEl.textContent = message || "";
}

function stopVideo(video) {
  video.pause();
  video.removeAttribute("src");
  video.load();
  video.hidden = true;
}

function playVideo(video, source) {
  video.hidden = false;
  video.src = source;
  video.currentTime = 0;
  video.muted = true;
  const playAttempt = video.play();
  if (playAttempt && typeof playAttempt.catch === "function") {
    playAttempt.catch(() => {});
  }
}

function hideMegatronWins() {
  stopVideo(winVideo);
  megatronWinsEl.hidden = true;
}

function hideGameOver() {
  stopVideo(resultVideo);
  gameOverEl.hidden = true;
}

function hideAllOverlays() {
  hideMegatronWins();
  hideGameOver();
}

function showMegatronWins() {
  hideGameOver();
  megatronWinsEl.hidden = false;
  playVideo(winVideo, "videos/win.mp4");
}

function showGameOver(title, message, videoSource) {
  hideMegatronWins();
  gameOverTitle.textContent = title;
  gameOverMessage.textContent = message;
  gameOverEl.hidden = false;
  if (videoSource) {
    playVideo(resultVideo, videoSource);
  } else {
    stopVideo(resultVideo);
  }
}

function scheduleMegatronWins() {
  const token = celebrationToken + 1;
  celebrationToken = token;
  if (celebrationTimer !== null) {
    clearTimeout(celebrationTimer);
  }
  celebrationTimer = setTimeout(() => {
    celebrationTimer = null;
    if (token !== celebrationToken) {
      return;
    }
    showMegatronWins();
  }, WIN_CELEBRATION_DELAY_MS);
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

  movesList.replaceChildren(fragment);
  if (movesList.scrollHeight > movesList.clientHeight) {
    movesList.scrollTop = movesList.scrollHeight;
  }
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

function setBoardPosition(fen, animate) {
  return new Promise((resolve) => {
    if (!window.board) {
      resolve();
      return;
    }

    const useAnimation = animate !== false && !prefersReducedMotion();
    if (!useAnimation) {
      if (pendingAnimationResolve) {
        const previous = pendingAnimationResolve;
        pendingAnimationResolve = null;
        previous();
      }
      window.board.position(fen, false);
      resolve();
      return;
    }

    let settled = false;
    const finish = () => {
      if (settled) {
        return;
      }
      settled = true;
      if (pendingAnimationResolve === finish) {
        pendingAnimationResolve = null;
      }
      resolve();
    };

    if (pendingAnimationResolve) {
      const previous = pendingAnimationResolve;
      pendingAnimationResolve = null;
      previous();
    }

    pendingAnimationResolve = finish;
    window.board.position(fen, true);
    window.setTimeout(finish, MOVE_ANIMATION_MS + 80);
  });
}

function resolveGameOverState() {
  if (window.game.in_checkmate()) {
    const losingSide = window.game.turn() === "w" ? "White" : "Black";
    const engineWon =
      (engineSide === "white" && losingSide === "Black") ||
      (engineSide === "black" && losingSide === "White");
    return {
      kind: engineWon ? "engine-win" : "human-win",
      losingSide,
    };
  }

  if (window.game.in_stalemate()) {
    return { kind: "draw", message: "The game ended in stalemate." };
  }

  if (window.game.in_draw()) {
    return { kind: "draw", message: "The game ended in a draw." };
  }

  return null;
}

function presentGameOver(result) {
  if (!result) {
    return false;
  }

  if (result.kind === "engine-win") {
    scheduleMegatronWins();
    return true;
  }

  if (result.kind === "human-win") {
    showGameOver(
      "You win",
      result.losingSide + " is checkmated.",
      "videos/lose.mp4"
    );
    return true;
  }

  showGameOver("Draw", result.message, "videos/lose.mp4");
  return true;
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

function continueAfterPlayerMove() {
  updateStatus();
  if (presentGameOver(resolveGameOverState())) {
    return;
  }
  requestEngineMove();
}

function applyPlayerMove(source, target, promotionPiece) {
  const moveOptions = {
    from: source,
    to: target,
  };
  if (promotionPiece) {
    moveOptions.promotion = promotionPiece;
  }

  const moveResult = window.game.move(moveOptions);
  if (!moveResult) {
    return false;
  }

  clearSquareSelection();
  return true;
}

async function requestEngineMove() {
  if (window.isThinking || !engineShouldMove()) {
    return;
  }

  const requestId = ++moveRequestId;
  const requestedDepth = searchDepth;
  const controller = new AbortController();
  activeMoveController = controller;

  setThinking(true);
  showError("");

  try {
    const response = await fetch("/api/move", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        moves: collectUciMoves(),
        depth: requestedDepth,
      }),
      signal: controller.signal,
    });

    const data = await response.json();
    if (requestId !== moveRequestId) {
      return;
    }

    if (!response.ok) {
      throw new Error(data.error || "Engine request failed");
    }

    if (data.depth != null && Number(data.depth) !== requestedDepth) {
      throw new Error(
        "Server used depth " + data.depth + " instead of " + requestedDepth
      );
    }

    if (!data.bestmove) {
      updateStatus();
      presentGameOver(resolveGameOverState());
      return;
    }

    const moveOptions = {
      from: data.bestmove.from,
      to: data.bestmove.to,
    };
    if (data.bestmove.promotion) {
      moveOptions.promotion = data.bestmove.promotion;
    }

    const played = window.game.move(moveOptions);
    if (!played) {
      throw new Error("Engine returned an illegal move");
    }

    await setBoardPosition(window.game.fen());
    if (requestId !== moveRequestId) {
      return;
    }

    updateStatus();
    presentGameOver(resolveGameOverState());
  } catch (error) {
    if (requestId !== moveRequestId || error.name === "AbortError") {
      return;
    }
    showError(error.message || "Failed to contact Megatron");
  } finally {
    if (activeMoveController === controller) {
      activeMoveController = null;
    }
    if (requestId === moveRequestId) {
      setThinking(false);
    }
  }
}

function onDragStart(source, piece) {
  clearSquareSelection();
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

function playerMaySelectSquare(square) {
  const piece = window.game.get(square);
  if (!piece || window.game.game_over() || window.isThinking) {
    return false;
  }
  if (
    (window.game.turn() === "w" && piece.color !== "w") ||
    (window.game.turn() === "b" && piece.color !== "b")
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

function choosePromotion(color) {
  return new Promise((resolve) => {
    pendingPromotion = resolve;
    promotionModal.hidden = false;
    promotionModal.replaceChildren();
    ["q", "r", "n", "b"].forEach((piece) => {
      const image = document.createElement("img");
      image.className = "promo-icon";
      image.dataset.piece = piece;
      image.src = `chesspieces/pieces/${color}${piece.toUpperCase()}.png`;
      image.alt = piece;
      image.draggable = false;
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

function onDrop(source, target) {
  ignoreBoardClickUntil = Date.now() + 250;
  afterSnapHandler = null;

  if (window.isThinking || window.game.game_over()) {
    return "snapback";
  }

  if (!target || target === "offboard") {
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
    const probeOptions = { from: source, to: target, promotion: "q" };
    const probe = window.game.move(probeOptions);
    if (!probe) {
      return "snapback";
    }
    window.game.undo();
    const color = piece.color === "w" ? "w" : "b";
    window.setTimeout(() => {
      choosePromotion(color).then((promotionPiece) => {
        if (!applyPlayerMove(source, target, promotionPiece)) {
          return;
        }
        window.board.position(window.game.fen(), !prefersReducedMotion());
        continueAfterPlayerMove();
      });
    }, 0);
    return "snapback";
  }

  if (!applyPlayerMove(source, target, null)) {
    return "snapback";
  }

  afterSnapHandler = continueAfterPlayerMove;
  return undefined;
}

function onSnapEnd() {
  window.board.position(window.game.fen(), false);
  const next = afterSnapHandler;
  afterSnapHandler = null;
  if (next) {
    next();
  }
}

function onMoveEnd() {
  if (pendingAnimationResolve) {
    const finish = pendingAnimationResolve;
    pendingAnimationResolve = null;
    finish();
  }
}

function handleBoardTap(square) {
  if (Date.now() < ignoreBoardClickUntil) {
    return;
  }
  if (window.isThinking || window.game.game_over() || pendingPromotion) {
    return;
  }

  if (!selectedSquare) {
    if (!playerMaySelectSquare(square)) {
      return;
    }
    selectedSquare = square;
    const squareEl = boardEl.querySelector('[data-square="' + square + '"]');
    if (squareEl) {
      squareEl.classList.add("is-selected");
    }
    return;
  }

  if (selectedSquare === square) {
    clearSquareSelection();
    return;
  }

  const source = selectedSquare;
  const piece = window.game.get(source);
  clearSquareSelection();
  if (!piece) {
    return;
  }

  const targetRank = square.charAt(1);
  const needsPromotion =
    piece.type === "p" && (targetRank === "8" || targetRank === "1");

  if (needsPromotion) {
    const probe = window.game.move({
      from: source,
      to: square,
      promotion: "q",
    });
    if (!probe) {
      return;
    }
    window.game.undo();
    choosePromotion(piece.color === "w" ? "w" : "b").then((promotionPiece) => {
      if (!applyPlayerMove(source, square, promotionPiece)) {
        return;
      }
      window.board.position(window.game.fen(), !prefersReducedMotion());
      continueAfterPlayerMove();
    });
    return;
  }

  if (!applyPlayerMove(source, square, null)) {
    return;
  }

  window.board.position(window.game.fen(), !prefersReducedMotion());
  continueAfterPlayerMove();
}

function onBoardClick(event) {
  const squareEl = event.target.closest("[data-square]");
  if (!squareEl || !boardEl.contains(squareEl)) {
    return;
  }
  handleBoardTap(squareEl.getAttribute("data-square"));
}

function fitBoardContainer() {
  if (!window.board) {
    return;
  }
  boardEl.style.width = "";
  window.board.resize();
  const innerBoard = boardEl.querySelector(".board-b72b1");
  if (!innerBoard) {
    return;
  }
  boardEl.style.width = innerBoard.getBoundingClientRect().width + "px";
}

function createBoard() {
  window.board = Chessboard("board", {
    draggable: true,
    position: "start",
    orientation: currentOrientation(),
    pieceTheme: "chesspieces/pieces/{piece}.png",
    moveSpeed: MOVE_ANIMATION_MS,
    snapbackSpeed: 60,
    snapSpeed: 40,
    appearSpeed: MOVE_ANIMATION_MS,
    trashSpeed: 60,
    onDrop,
    onDragStart,
    onSnapEnd,
    onMoveEnd,
  });
  fitBoardContainer();
}

function resetGame() {
  cancelActiveMoveRequest();
  cancelCelebration();
  clearSquareSelection();
  afterSnapHandler = null;
  hideAllOverlays();
  showError("");
  setThinking(false);
  window.game = new Chess();

  if (window.board) {
    window.board.orientation(currentOrientation());
    window.board.position("start", false);
    fitBoardContainer();
  } else {
    createBoard();
  }

  updateStatus();
  if (engineShouldMove()) {
    requestEngineMove();
  }
}

sideSelect.addEventListener("change", (event) => {
  engineSide = event.target.value;
  resetGame();
});

depthSlider.addEventListener("input", (event) => {
  syncDepthDisplay(event.target.value);
});

function getAboutFocusable() {
  return aboutCard.querySelectorAll(
    'a[href], button:not([disabled]), [tabindex]:not([tabindex="-1"])'
  );
}

function openAboutModal() {
  if (aboutOpen) {
    return;
  }
  aboutOpen = true;
  aboutLastFocus = document.activeElement;
  aboutModal.hidden = false;
  document.body.classList.add("about-open");
  aboutButton.setAttribute("aria-expanded", "true");
  window.setTimeout(() => {
    aboutClose.focus();
  }, 0);
}

function closeAboutModal() {
  if (!aboutOpen) {
    return;
  }
  aboutOpen = false;
  aboutModal.hidden = true;
  document.body.classList.remove("about-open");
  aboutButton.setAttribute("aria-expanded", "false");
  if (aboutLastFocus && typeof aboutLastFocus.focus === "function") {
    aboutLastFocus.focus();
  } else {
    aboutButton.focus();
  }
}

function onAboutKeyDown(event) {
  if (!aboutOpen) {
    return;
  }

  if (event.key === "Escape") {
    event.preventDefault();
    closeAboutModal();
    return;
  }

  if (event.key !== "Tab") {
    return;
  }

  const focusable = Array.from(getAboutFocusable());
  if (focusable.length === 0) {
    return;
  }

  const first = focusable[0];
  const last = focusable[focusable.length - 1];
  if (event.shiftKey && document.activeElement === first) {
    event.preventDefault();
    last.focus();
  } else if (!event.shiftKey && document.activeElement === last) {
    event.preventDefault();
    first.focus();
  }
}

resetButton.addEventListener("click", resetGame);
gameOverReset.addEventListener("click", resetGame);
dismissWins.addEventListener("click", hideMegatronWins);
winsPlayAgain.addEventListener("click", resetGame);
boardEl.addEventListener("click", onBoardClick);
aboutButton.addEventListener("click", openAboutModal);
aboutClose.addEventListener("click", closeAboutModal);
aboutModal.addEventListener("click", (event) => {
  if (event.target && event.target.getAttribute("data-about-close") === "true") {
    closeAboutModal();
  }
});
document.addEventListener("keydown", onAboutKeyDown);

window.addEventListener("resize", () => {
  fitBoardContainer();
});

document.addEventListener("DOMContentLoaded", () => {
  engineSide = sideSelect.value;
  syncDepthDisplay(depthSlider.value || DEFAULT_DEPTH);
  aboutButton.setAttribute("aria-expanded", "false");
  window.game = new Chess();
  createBoard();
  updateStatus();
});
