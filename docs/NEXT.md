# What's next for Clam

Roadmap from the current LAN + ghost prototype toward secure friend multiplayer.

**Current release:** v1.1.3 — LAN discovery, WebSocket lobby, ghost avatars on main 1–22.

---

## What works today

| Feature | Status |
|---------|--------|
| Clam menu on main menu | Done |
| Host WebSocket server (`0.0.0.0:8765`) | Done |
| Join via **Nearby on LAN** (tap-to-join, no IP) | Done |
| UDP discovery beacon (port 8766) | Done |
| Username in beacon / hello (optional setting) | Done |
| Peer list + event console | Done |
| Host visible on LAN only when in a level | Done |
| Ghost avatars on main campaign 1–22 (classic) | Done |
| `player_state` position sync ~20 Hz | Done |
| PlayLayer hook + `SimplePlayer` ghosts | Done |
| Cross-platform CI + GitHub Releases | Done |
| Persistent LAN browser (runs from mod load) | Done |
| Linux/macOS subnet broadcast | Done |

### In source, not yet released

| Feature | Status |
|---------|--------|
| Ghost interpolation (smooth movement) | Implemented |
| Icon scale sync (mini support) | Implemented |
| Primary/secondary color sync | Implemented |

---

## What does not work yet

| Feature | Status |
|---------|--------|
| Join authentication / room codes | Not started |
| GD friends-only gate | Not started |
| Encrypted WebSocket traffic | Not started |
| Gamemode / flip / ship-ufo-ball sync | Not started |
| Glow color sync | Not started |
| Coordinated level start (`start_level` flow) | Not started |
| Internet play (non-LAN) | Not planned for v1 |

---

## Phase 2 — Secure lobby (next priority)

Goal: strangers on the same Wi‑Fi can't join; friends can join reliably.

WebSocket stays the transport. Security is added as a **handshake before lobby messages**.

### 2a. Room code auth

- Host generates a random room code when clicking **Host** (show in UI: “Share code: `AB12CD`”).
- Client sends `{ type: "auth", accountId, name, roomCode, nonce }` before `hello`.
- Host rejects wrong code.
- UDP beacons stay **untrusted** (advertise port only; no secrets on LAN broadcast).

**Success criteria:** random device on same Wi‑Fi cannot join without the code.

### 2b. GD friends-only (soft filter)

- Read local friend list via `GameLevelManager` / `GJAccountManager::m_accountID`.
- Mod setting: **Friends only** (default off for dev, on for release).
- Host rejects `auth` if `accountId` is not in cached friends list.

**Limitation:** account ID can be spoofed without cryptographic proof. Pair with room code.

### 2c. Session token

- After successful `auth`, host replies `{ type: "auth_ok", sessionToken }`.
- All later messages include the token.

### 2d. Message integrity (LAN privacy)

- Derive session key from room code + nonce.
- HMAC or AES-GCM on JSON payloads after auth.

**Defer:** `wss://` until internet relay exists.

---

## Phase 3 — Gameplay polish (partially done)

Goal: ghosts look and feel like the real player.

| Item | Status |
|------|--------|
| PlayLayer hooks + `player_state` | Done (v1.1.0) |
| Non-colliding ghost `SimplePlayer` | Done |
| Interpolation | In source |
| Icon + scale + colors | In source |
| Gamemode / flip / mini visual state | Not started |
| `start_level` coordinated load | Not started |

**Explicitly out of scope for v1:** shared death, checkpoints, 2P mechanics, object interaction.

---

## Phase 4 — Lobby polish

- UI: show room code prominently on host screen
- Copy-to-clipboard for room code
- Latency display per peer
- “Leave game” without closing GD
- Better empty states

---

## Phase 5 — Later

| Idea | Notes |
|------|--------|
| Internet relay | Central or host-forwarded; needs `wss://` and NAT traversal |
| Strong account auth | RobTop session validation or auth server |
| Voice / emotes | After core sync is stable |
| Shared 2P levels | Authoritative gameplay — much harder |
| Broader level support | Daily/weekly/custom levels |
| Android/iOS LAN quirks | Test discovery on mobile networks |

---

## Suggested build order

```
Release v1.1.4   Interpolation + scale + colors (ship current source)
Phase 2a         Room code auth          ← next major feature
Phase 2b         Friends-only setting
Phase 2c         Session token
Phase 3          Gamemode sync, start_level flow
Phase 4          UI polish
Phase 2d         Payload encryption     ← optional before public release
Phase 5          Internet / strong auth
```

Do **not** skip Phase 2 before wide public release — anyone on the LAN can join today.

---

## Security model summary

| Layer | Purpose |
|-------|---------|
| UDP beacon | Find hosts (untrusted) |
| WebSocket | Session + game data |
| Room code | Authorization |
| Friend list | Soft authorization |
| Session token | Connection binding |
| App-layer crypto | LAN confidentiality |
| `wss://` | Internet deployment |

**Username on LAN:** public GD name via **Share Username on LAN** setting. Never send passwords or session secrets.

---

## Protocol sketch (target)

```
# Discovery (UDP, untrusted) — implemented
{ "type": "clam_beacon", "protocol": 1, "hostName", "wsPort", "players", "levelId" }

# Auth (WebSocket, required first) — not implemented
→ { "type": "auth", "accountId", "name", "roomCode", "nonce" }
← { "type": "auth_ok", "sessionToken", "sessionKey" }

# Lobby — implemented
→ { "type": "hello", "name" }
← { "type": "lobby", "players": [{ "id", "name" }] }

# Gameplay — partially implemented
→ { "type": "level_start", "peerId", "levelId" }
→ { "type": "level_end", "peerId", "levelId" }
→ { "type": "player_state", "peerId", "levelId", "x", "y", "rotation", "dead", "iconId", "scale", "color1", "color2" }
← { "type": "start_level", "levelId" }   # future
```

Full current protocol: [ARCHITECTURE.md](ARCHITECTURE.md).

---

## Testing checklist

### Ghost sync (v1.1.x)

- [x] Host + one client in same level
- [x] Ghost visible with icon
- [ ] Ghost colors match (after v1.1.4)
- [ ] Movement smooth with interpolation (after v1.1.4)
- [x] Disconnect removes ghost
- [x] Local gameplay unaffected

### Security (Phase 2)

- [ ] Join without room code → rejected
- [ ] Join with wrong code → rejected
- [ ] Join with correct code → lobby works
- [ ] Friends-only on → non-friend rejected

### LAN discovery

- [x] Mac host visible to Linux client (subnet broadcast)
- [x] Host not listed until in level
- [x] Browser works without popup open

---

## Related docs

- [ARCHITECTURE.md](ARCHITECTURE.md) — threads, modules, protocol
- [DEV-SHARING.md](DEV-SHARING.md) — build and release
- [../changelog.md](../changelog.md) — shipped versions

---

## Release tagging reminder

1. Bump `version` in `mod.json`
2. Update `changelog.md`
3. `git tag v<version> && git push origin v<version>`
4. GitHub Actions publishes `paxcirlot.clam.geode`
