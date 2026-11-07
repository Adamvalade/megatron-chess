FROM gcc:13-bookworm AS engine-build
WORKDIR /src
COPY Makefile Piece.cpp Piece.h board.cpp board.h engine.cpp engine.h eval.cpp eval.h search.cpp search.h tt.cpp tt.h uci.cpp uci.h zobrist.cpp zobrist.h magic.h pstTables.h utilities.h tests.h ./
RUN make release

FROM node:20-bookworm-slim
WORKDIR /app
RUN apt-get update && apt-get install -y --no-install-recommends libstdc++6 && rm -rf /var/lib/apt/lists/*
COPY --from=engine-build /src/engine /app/engine
COPY node_chess/package.json node_chess/package-lock.json ./node_chess/
WORKDIR /app/node_chess
RUN npm ci --omit=dev
COPY node_chess/ ./
ENV ENGINE_PATH=/app/engine
ENV NODE_ENV=production
EXPOSE 3000
CMD ["node", "server.js"]
