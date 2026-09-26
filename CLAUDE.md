# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

High-concurrency instant messaging chat server built on the **Drogon** framework (C++20), with a React/TypeScript frontend. Supports private chat, group chat, AI chatbot, friend management, real-time notifications, and file uploads.

## Build & Run

### Docker (production)

```bash
cd ChatServer
docker compose build         # Build the chat image
docker compose up -d          # Start postgres + redis + chat + nginx
docker compose logs -f chat   # Follow server logs
```

The server listens on port **10086** internally; Nginx on port **80** proxies to it.

### Local / offline Docker rebuild

```bash
docker build -f Dockerfile.local -t chat:latest ..
```

### CMake (dev workstation)

Requires: Drogon, hiredis, libcurl, OpenSSL, libpq, libsqlite3, jsoncpp, UUID, and jwt-cpp headers (in `third_party/jwt-cpp/include`).

```bash
cd ChatServer
mkdir -p build && cd build
cmake .. -DCMAKE_CXX_STANDARD=20 -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
# Binary: build/Test
```

**Important:** The project uses **Clang** (not GCC) to work around a GCC 13 ICE in coroutine frame generation. In a VS Code + CMake workflow, set `CC=clang CXX=clang++`.

### Visual Studio

Open `ChatServer.sln` (VS 2022, Debug/Release x64). The vcxproj is at `ChatServer/ChatServer.vcxproj`.

### Tests

```bash
python test/api_test.py                      # API tests
cd test/load && locust -f ws_locustfile.py   # Load tests (Locust)
```

## Architecture

### Layered design (top to bottom)

```
HTTP / WebSocket  ── controllers/           (Drogon HttpController / WebSocketController)
       │
   filter/          TokenVerifyFilter        (JWT validation, registered on protected routes)
   middleware/      CORSMiddleware
       │
   Service/         UserService, MessageService, ThreadService,
                    ConnectionService, RelationshipService, RedisService,
                    ClusterService
       │
   Data/            Repository interfaces (IUserRepository, IMessageRepository, …)
                    + Postgres* implementations + SQLite/PostgreSQL initializers
       │
   models/          Drogon ORM model objects (Users, Messages, Threads, GroupChats, …)
```

**Dependency injection** is handled by `Container` (singleton, service locator pattern). It owns all repositories and services and is initialized eagerly via `Container::GetInstance()` during `registerBeginningAdvice`.

### Auth flow

- **JWT dual-token**: Access token (short TTL, 15 min) + Refresh token (long TTL, 14 days), both carry `{ uid, typ, jti, exp }`. User profile data is resolved from Redis/DB after verification, never embedded in tokens.
- **Token rotation**: On refresh, old refresh token family is invalidated (reuse detection).
- Auth code lives in `auth/` — `TokenService` (sign/verify), `TokenFactory` (generate pairs), `PasswordService` (bcrypt hashing), `SecretProvider` (reads `jwt_secret.json`).
- HTTP endpoints use `TokenVerifyFilter`; WebSocket uses `ConnectionService` with per-connection token state.

### WebSocket protocol

All WebSocket messages go through a single endpoint `/ws/chat`, handled by `ChatController`. Message types are defined in `Common/WsProtocol.h`:

- `1` Heartbeat (C→S), `2` HeartbeatAck (S→C)
- `3` TokenRefresh (C→S), `4` TokenRefreshed (S→C)
- `5` ChatSend (C→S), `6` ChatMessage (S→C)
- `7` AiRequest (C→S), `8` AiResponse (S→C, streaming)
- `9` Notice (S→C, with sub_type), `10` Error (S→C, with sub_type + error code)

### Connection management

`ConnectionService` maintains an in-memory `uid → WebSocketConnectionPtr` map protected by a mutex. Key behaviors:

- On connect: user's Redis online-set is updated and a user route (`route:user:{uid} = nodeId`) is written; offline Redis messages are flushed.
- On disconnect: a timer starts; if user reconnects within the window, the timer is canceled. Otherwise the user is marked offline, the route is cleared, and the user is kicked in other services.
- Heartbeat monitor periodically scans for zombie connections and sends expiry warnings.
- Token refresh happens over WebSocket (type `3` / `4`).

### Multi-node routing (`ClusterService`)

- Each node has a unique `node_id` (from `NODE_ID` env / `config.json` `node.id` / hostname+random) and subscribes to its own channel `node:channel:{nodeId}`.
- Liveness: the node writes `node:alive:{nodeId}` (TTL 30s) every 10s.
- `ConnectionService::DeliverToUser` order: local WebSocket → cross-node `PUBLISH` (if `route:user:{uid}` points to a live node) → Redis offline queue. `PUBLISH` returning zero subscribers is treated as "target dead" and falls back to the offline queue, so stale routes never drop messages.
- Incoming cross-node messages are handled by a `LocalDeliverer` callback that calls `ConnectionService::DeliverLocal`.

### Key data flows

- **Chat send**: WebSocket type `5` → `MessageService::DispatchMessage()` serializes and pushes to receiver's WebSocket if online; otherwise queues to Redis.
- **Reconnect/resync**: Client calls `GET /thread/record/overview` with `existing_id` → server returns all missed messages across all threads the user is in.
- **AI chat**: WebSocket types `7`/`8` with streaming responses. AI context stored in `ai_context` table.

### Database

- **Primary**: PostgreSQL (via Drogon ORM in `models/` + raw SQL in `Data/Postgres*`). Configured in `config.json` under `db_clients`.
- **Fallback/offline**: SQLite schema defined in `const.h`. `SQLiteInitializer` reads from `database.db` for local/dev use.
- **Cache**: Redis for online user sets and offline message queues.

### Frontend (React/TypeScript)

`src/` contains a React app (components, pages, stores, services, types, utils, styles). The C++ server serves it as static files (`static/` directory, set as `documentRoot` in Drogon). In development, the Vite dev server (port 5173) proxies to the backend — the nginx config reflects this CORS setup.

## Configuration

- **`config.json`**: Drogon server settings — listeners, PostgreSQL connection, Redis host/port, upload path. Mounted read-only in the Docker container so changes take effect on restart.
- **`jwt_secret.json`**: JWT signing keys. Also mounted read-only. Do not commit real secrets.
- **`nginx.conf`**: Reverse proxy on port 80 → chat:10086, with CORS for localhost:5173.

## Error codes

Defined in `const.h` under `namespace ChatCode::Code`. Key ranges: `100-199` general, `200-299` relationship-specific. The `Code` enum is used in WebSocket `Error` messages (type `10`, sub_type from `ErrorSubType`).

## Third-party dependencies

- **jwt-cpp** (header-only, in `third_party/jwt-cpp/include/`)
- **Drogon** + **trantor** (web framework, non-blocking I/O)
- **hiredis** (Redis client)
- **libpq** (PostgreSQL client)
- **jsoncpp** (JSON parsing)
- **OpenSSL** (TLS, crypto primitives)
- **libcurl** (HTTP client, e.g., AI API calls)
