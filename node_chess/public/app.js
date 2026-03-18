window.board = null;
window.game = null;

let engineSide = 'black';

const $status = $('#status');
const $fen = $('#fen');
const $pgn = $('#pgn');


document.addEventListener('touchstart', () => {}, { passive: false });
document.addEventListener('contextmenu', (e) => {
  if (e.target.nodeName === 'IMG') e.preventDefault();
}, false);


function updateColor() {
  engineSide = document.getElementById('sideSelect').value;
}


function updateDifficultyLabel() {
  document.getElementById('difficultyValue').innerText =
    document.getElementById('difficulty').value;
}


function makeEngineMoveIfNeeded() {
  if (game.game_over()) {
    console.log('Game over (no further engine moves)');
    return;
  }
  const turn = game.turn();
  if (engineSide === 'both') {
    callEngineAndMove();
  } else if (engineSide === 'white' && turn === 'w') {
    callEngineAndMove();
  } else if (engineSide === 'black' && turn === 'b') {
    callEngineAndMove();
  }
}


function renderMoves() {
  const list = document.getElementById('moves-list');
  if (!list) return;

  // Get SAN with move numbers
  const verbose = window.game.history({ verbose: true });

  // Build rows as pairs: one white SAN, one black SAN
  // Each object includes san + ply index (0-based half-move index)
  const pairs = [];
  for (let i = 0; i < verbose.length; i += 2) {
    const white = verbose[i]  ? { san: verbose[i].san,  ply: i }   : null;
    const black = verbose[i + 1] ? { san: verbose[i + 1].san, ply: i + 1 }: null;
    pairs.push({ index: Math.floor(i / 2) + 1, white, black });
  }

  // Build DOM
  const frag = document.createDocumentFragment();
  pairs.forEach(({ index, white, black }) => {
    const row = document.createElement('li');
    row.className = 'move-row';

    const idx = document.createElement('div');
    idx.className = 'move-index';
    idx.textContent = index + '.';
    row.appendChild(idx);

    const w = document.createElement('div');
    w.className = 'move';
    w.textContent = white ? white.san : '';
    if (white) w.dataset.ply = white.ply;
    row.appendChild(w);

    const b = document.createElement('div');
    b.className = 'move';
    b.textContent = black ? black.san : '';
    if (black) b.dataset.ply = black.ply;
    row.appendChild(b);

    frag.appendChild(row);
  });

  list.innerHTML = '';
  list.appendChild(frag);

  // Auto-scroll to latest
  list.parentElement.scrollTop = list.parentElement.scrollHeight;
}


function flipSquare(square) {
  const files = "abcdefgh";
  const file = square.charAt(0);
  const rank = square.charAt(1);
  const flippedFile = files[files.length - 1 - files.indexOf(file)];
  const flippedRank = (9 - parseInt(rank)).toString();
  return flippedFile + flippedRank;
}

function flipMove(move) {
  if (move.length < 4) return move;
  return flipSquare(move.slice(0,2)) + flipSquare(move.slice(2,4)) + (move.slice(4) || "");
}


function hardDOMCleanup() {
  const ghostDiv = document.createElement('div');
  document.body.appendChild(ghostDiv);
  ghostDiv.offsetHeight; // force reflow
  document.body.removeChild(ghostDiv);
}


function callEngineAndMove() {

  if (window.isThinking) return;

  window.isThinking = true; 

  const localTurn = game.turn();
  const localSide = engineSide;
  let localShouldFlip = (localSide === 'black' && localTurn === 'b');

  let moves = game.history({ verbose: true })
    .map(m => m.from + m.to + (m.promotion || ''));

  if (localShouldFlip) {
    moves = moves.map(move => flipMove(move));
  }

  console.log("Sending to engine:", moves);

  fetch('/move', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ 'moves[]': moves })
  })
  .then(r => r.json())
  .then(data => {
    if (!data.bestmove) {
      console.log("No bestmove received");
      return;
    }

    let bestmove = data.bestmove;
    if (localShouldFlip) {
      bestmove = {
        from: flipSquare(bestmove.from),
        to: flipSquare(bestmove.to),
        promotion: bestmove.promotion || ''
      };
    }

    console.log('Got engine move:', bestmove);
    window.game.move(bestmove);
    requestAnimationFrame(() => {
      window.board.position(window.game.fen());
    });
    updateStatus();

  })
  .catch(err => {
    console.error("Error in POST /move:", err);
  })
  .finally(() => {
    window.isThinking = false;
  });
}


function onDragStart(source, piece) {
  if (window.game.game_over()) return false;
  if (window.isThinking) return false;
  // chessboard.js calls onDragStart(source, piece, position, orientation)
  // Only allow dragging the side that is to move (piece is e.g. 'wK' or 'bP')
  if (!piece || (typeof piece !== 'string')) return false;
  if ((window.game.turn() === 'w' && piece.indexOf('b') === 0) ||
      (window.game.turn() === 'b' && piece.indexOf('w') === 0)) {
    return false;
  }
}


function handleUserMove(source, target, promotionPiece = '') {
  const moveResult = window.game.move({
    from: source,
    to: target,
    promotion: promotionPiece
  });
  if (!moveResult) {
    window.isThinking = false;
    return 'snapback';
  }

  requestAnimationFrame(() => {
    window.board.position(window.game.fen());
  });
  
  updateStatus();

  if (window.game.in_checkmate() || window.game.in_draw()) {
    const loser = window.game.turn(); 
    const engineWon = (
      (engineSide === 'white' && loser === 'b') ||
      (engineSide === 'black' && loser === 'w') ||
      (engineSide === 'both') 
    );

    const engineLost = (
      (engineSide === 'black' && loser === 'b') ||
      (engineSide === 'white' && loser === 'w')
    );

    const overlay = document.getElementById(
      window.game.in_draw()
        ? 'draw-overlay'
        : engineWon
        ? 'highlight-overlay'
        : 'loser-overlay'
    );
    const video = document.getElementById('highlight-video');

    setTimeout(() => {
      overlay.style.display = 'flex';
      video.currentTime = 0;
      video.muted = true;
      video.play().then(() => {
        console.log("video playing")
      }).catch((err) => {
        console.error("Autoplay failed:", err);
      });

      setTimeout(() => {
        overlay.style.display = 'none';
      }, 10000);

      video.onended = () => {
        overlay.style.display = 'none';
      };
    }, 1000); // wait 1 sec

    window.isThinking = false;
    return; 
  }

  const moves = window.game.history({ verbose: true })
    .map(m => m.from + m.to + (m.promotion || ''));

  fetch('/move', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ 'moves[]': moves })
  })
    .then(async r => {
      const text = await r.text();
      console.log("Raw response from /move:", text);
      return JSON.parse(text);
    })
    .then(data => {
      if (data.bestmove) {
        window.game.move({
          from: data.bestmove.from,
          to: data.bestmove.to,
          promotion: data.bestmove.promotion || ''
        });
        requestAnimationFrame(() => {
          window.board.position(window.game.fen());
        });
        updateStatus();

        if (window.game.in_checkmate()) {
          console.log("game over, displaying video");
          const loser = window.game.turn(); 
          const engineWon = (
            (engineSide === 'white' && loser === 'b') ||
            (engineSide === 'black' && loser === 'w') ||
            (engineSide === 'both') 
          );
  
          const engineLost = (
            (engineSide === 'black' && loser === 'b') ||
            (engineSide === 'white' && loser === 'w')
          );
    
          if (engineWon) {
            setTimeout(() => {
              const overlay = document.getElementById('highlight-overlay');
              const video = document.getElementById('highlight-video');
              overlay.style.display = 'flex';
              video.currentTime = 0;
              video.muted = true;
              video.play().then(() => {
                console.log("video playing")
              }).catch((err) => {
                console.error("Autoplay failed:", err);
              });
  
              setTimeout(() => {
                overlay.style.display = 'none';
              }, 10000);
          
              video.onended = () => {
                overlay.style.display = 'none';
              };
            }, 1000); // wait 1 second
          }
  
          else if (window.game.in_draw()) {
            setTimeout(() => {
              const overlay = document.getElementById('draw-overlay');
              const video = document.getElementById('highlight-video');
              overlay.style.display = 'flex';
              video.currentTime = 0;
              video.muted = true;
              video.play().then(() => {
                console.log("video playing")
              }).catch((err) => {
                console.error("Autoplay failed:", err);
              });
  
              setTimeout(() => {
                overlay.style.display = 'none';
              }, 10000);
          
              video.onended = () => {
                overlay.style.display = 'none';
              };
            }, 1000); // wait 1 second
  
          }
          else if (engineLost) {
            setTimeout(() => {
              const overlay = document.getElementById('loser-overlay');
              const video = document.getElementById('highlight-video');
              overlay.style.display = 'flex';
              video.currentTime = 0;
              video.muted = true;
              video.play().then(() => {
                console.log("video playing")
              }).catch((err) => {
                console.error("Autoplay failed:", err);
              });
  
              setTimeout(() => {
                overlay.style.display = 'none';
              }, 10000);
          
              video.onended = () => {
                overlay.style.display = 'none';
              };
            }, 1000); // wait 1 second
          }
        }
      }
    })
    .catch(err => {
      console.error("Error in POST /move response:", err);
    })
    .finally(() => {
      window.isThinking = false;
    });
}



async function onDrop(source, target) {
  if (window.isThinking) return 'snapback';
  window.isThinking = true;

  const piece = window.game.get(source)?.type;
  const isPawn = (piece === 'p');
  const targetRank = target.charAt(1);

  if (isPawn && (targetRank === '8' || targetRank === '1')) {
    // Try silent move
    const legalMove = window.game.move({
      from: source,
      to: target,
      promotion: 'q'
    });

    if (!legalMove) return 'snapback';

    // Undo test move
    window.game.undo();

    // Show menu and wait
    const promotionPiece = await showPromotionMenu(source, target);
    return handleUserMove(source, target, promotionPiece);
  }

  return handleUserMove(source, target, '');
}


function showPromotionMenu(source, target) {
  const modal = document.getElementById("promotion-modal");
  if (!modal) return;

  modal.style.display = "flex";

  const color = window.game.turn() === 'w' ? 'w' : 'b';
  modal.innerHTML = `
    <img class="promo-icon" data-piece="q" src="chesspieces/pieces/${color}Q.png" />
    <img class="promo-icon" data-piece="r" src="chesspieces/pieces/${color}R.png" />
    <img class="promo-icon" data-piece="n" src="chesspieces/pieces/${color}N.png" />
    <img class="promo-icon" data-piece="b" src="chesspieces/pieces/${color}B.png" />
  `;

  document.querySelectorAll(".promo-icon").forEach(img => {
    img.onclick = () => {
      const promotion = img.dataset.piece;
      modal.style.display = 'none';
      handleUserMove(source, target, promotion);
    };
  });
}


function setupPromotionClicks() {
  document.querySelectorAll('.promo-piece').forEach(img => {
    img.addEventListener('click', (e) => {
      const piece = e.target.dataset.piece;
      const menu = document.getElementById('promotion-menu');
      menu.style.display = 'none';
      if (window.resolvePromotion) window.resolvePromotion(piece);
    });
  });
}


function onSnapEnd() {
  requestAnimationFrame(() => {
    window.board.position(window.game.fen());
  });
}


function resetGame() {
  window.game.reset();

  destroyBoard();
  hardDOMCleanup();

  window.board = Chessboard('board', {
    draggable: true,
    position: 'start',
    pieceTheme: 'chesspieces/pieces/{piece}.png',
    onDrop : onDrop,
    onDragStart : onDragStart,
    onSnapEnd : onSnapEnd,
    pieceDragThreshold: 5
  });
  installIOSLongPressFix();

  if (engineSide === 'black') {
    window.board.orientation('white');
  } else if (engineSide === 'white') {
    window.board.orientation('black');
  } else {
    window.board.orientation('white');
  }

  window.board.position('start');

  if (engineSide === 'white' || engineSide === 'both') {
    makeEngineMoveIfNeeded();
  }
  window.isThinking = false;
  updateStatus();
}


function updateStatus() {
  let status = '';
  let moveColor = (window.game.turn() === 'b') ? 'Black' : 'White';

  if (window.game.in_checkmate()) {
    status = 'Game over, ' + moveColor + ' is in checkmate.';
  } else if (window.game.in_draw()) {
    status = 'Game over, drawn position';
  } else {
    status = moveColor + ' to move';
    if (window.game.in_check()) {
      status += ', ' + moveColor + ' is in check';
    }
  }

  $status.html(status);
  $fen.html(window.game.fen());
  $pgn.html(window.game.pgn());

  renderMoves();
}


function destroyBoard() {
  if (window.board) {
    const boardElement = document.getElementById('board');
    if (boardElement) {
      boardElement.innerHTML = '';
    }
    window.board = null;
  }
}


document.addEventListener('DOMContentLoaded', () => {

  console.log("TESTING: DOM fully loaded.");

  if (window.game) {
    delete window.game;
  }
  window.game = new Chess();
  
  destroyBoard();
  hardDOMCleanup(); // for safari

  window.board = Chessboard('board', {
    draggable: true,
    position: 'start',
    pieceTheme: 'chesspieces/pieces/{piece}.png',
    onDrop : onDrop,
    onDragStart : onDragStart,
    onSnapEnd : onSnapEnd,
    pieceDragThreshold: 5
  });
  installIOSLongPressFix();

  window.board.orientation('white');
  updateStatus();

  document.getElementById('sideSelect')
      .addEventListener('change', (e) => {
          engineSide = e.target.value; 
          resetGame();
  });

    console.log("Board created:", window.board);
});


// Disallow png image sheet being shown on IOS
function installIOSLongPressFix() {
  const boardEl = document.getElementById('board');
  if (!boardEl) return;

  const cancelIfPiece = (e) => {
    const t = e.target;
    if (!t) return;
    if (t.tagName === 'IMG' || t.classList.contains('piece-417db')) {
      e.preventDefault();  
    }
  };

  boardEl.addEventListener('touchstart',  cancelIfPiece, { passive: false, capture: true });
  boardEl.addEventListener('touchend',    cancelIfPiece, { passive: false, capture: true });
  boardEl.addEventListener('touchcancel', cancelIfPiece, { passive: false, capture: true });

  boardEl.addEventListener('dragstart', (e) => e.preventDefault(), true);
}