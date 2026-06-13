# What's next for Clam

Roadmap from the current LAN lobby prototype to friend multiplayer with visible avatars.

**Current release:** v1.0.1 — LAN discovery, WebSocket host/join, peer list, console.

---

## What works today

| Feature | Status |
|---------|--------|
| Clam menu on main menu | Done |
| Host WebSocket server (`0.0.0.0:8765`) | Done |
| Join by IP or **Nearby on LAN** | Done |
| UDP discovery beacon (port 8766) | Done |
| Username in beacon / hello (optional setting) | Done |
| Peer list + event console | Done |
| Cross-platform CI + GitHub Releases | Done |

## What does not work yet

| Feature | Status |
|---------|--------|
| Join authentication / room codes | Not started |
| GD friends-only gate | Not started |
| Encrypted WebSocket traffic | Not started |
| In-level ghost avatars | Not started |
| Player position / gamemode sync | Not started |
| Start level together | Not started |
| Internet play (non-LAN) | Not planned for v1 |

---

## Phase 2 — Secure lobby (do this before heavy gameplay sync)

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

**Limitation:** account ID can be spoofed without cryptographic proof. This stops casual joins, not a determined attacker. Pair with room code.

### 2c. Session token (optional but recommended)

- After successful `auth`, host replies `{ type: "auth_ok", sessionToken }`.
- All later messages include the token.
- Host drops connections that skip auth or use invalid tokens.

### 2d. Message integrity (LAN privacy)

- Derive a short **session key** from room code + nonce (or random key in `auth_ok`).
- HMAC or AES-GCM on JSON payloads after auth.
- Plain `ws://` on LAN is OK if payloads are encrypted at app layer.

**Defer:** `wss://` until internet relay exists (needs cert/trust story).

See [Security model](#security-model-summary) below.

---

## Phase 3 — See each other in a level (core multiplayer milestone)

Goal: two friends in the same level see each other's cube/ship moving.

### 3a. PlayLayer hooks

- Hook `PlayLayer` update tick.
- Read local `PlayerObject`: position, rotation, gamemode, flip, mini.
- Send `{ type: "player_state", ... }` at ~20–30 Hz on the authenticated WebSocket.

### 3b. Remote ghost players

- On remote `player_state`, spawn/update a **non-colliding** ghost `PlayerObject`.
- Apply icon/colors from profile sent at auth/hello (`copyAttributes` or equivalent).
- Interpolate between network updates to reduce jitter.
- Remove ghost on disconnect.

### 3c. Level agreement

- Host picks a level in lobby → sends `{ type: "start_level", levelId, levelName }`.
- Clients load the same level locally (each runs own physics — visual sync only for v1).

**Success criteria:** two clients in the same level see each other's avatars move in real time with correct colors/icon.

**Explicitly out of scope for v1:** shared death, checkpoints, 2P mechanics, object interaction.

---

## Phase 4 — Lobby polish

- UI: show room code prominently on host screen.
- UI: copy-to-clipboard for room code (where platform allows).
- Nearby list: show room code hint or “friends only” badge.
- Latency display per peer.
- “Leave game” without closing GD.
- Better empty states (“No friends nearby — share your room code”).

---

## Phase 5 — Later (maybe)

| Idea | Notes |
|------|--------|
| Internet relay | Central or host-forwarded; needs `wss://` and NAT traversal |
| Strong account auth | RobTop session validation or Globed-style auth server |
| Voice / emotes | After core sync is stable |
| Shared 2P levels | Requires authoritative gameplay — much harder |
| Android/iOS LAN quirks | Test discovery on mobile networks |

---

## Suggested build order

```
Phase 2a  Room code auth          ← next implementation step
Phase 2b  Friends-only setting
Phase 2c  Session token
Phase 3a  player_state messages
Phase 3b  Ghost PlayerObjects
Phase 3c  start_level flow
Phase 4   UI polish
Phase 2d  Payload encryption     ← optional, before public release
Phase 5   Internet / strong auth ← only if LAN product is solid
```

Do **not** jump to avatar sync before Phase 2a — you'll be syncing gameplay for anyone on the LAN who clicks Join.

---

## Security model summary

| Layer | Purpose | Spoofing / MITM |
|-------|---------|-----------------|
| UDP beacon | Find hosts | Untrusted; identity not verified here |
| WebSocket | Session + game data | Trusted after auth handshake |
| Room code | Authorization | Stops random LAN joiners |
| Friend list | Authorization (soft) | Blocks wrong accounts if not spoofed |
| Session token | Connection binding | Stops unauthenticated WS messages |
| App-layer crypto | Confidentiality on LAN | Helps MITM on local network |
| `wss://` | Transport crypto | For internet deployment |

**Username on LAN:** public GD name; toggle via **Share Username on LAN** mod setting. Never send passwords or session secrets.

---

## Protocol sketch (target)

```
# Discovery (UDP, untrusted)
{ "type": "clam_beacon", "protocol": 1, "hostName", "wsPort", "players" }

# Auth (WebSocket, required first)
→ { "type": "auth", "accountId", "name", "roomCode", "nonce" }
← { "type": "auth_ok", "sessionToken", "sessionKey" }

# Lobby (existing, gated by token)
→ { "type": "hello", ... }
← { "type": "lobby", "players": [...] }

# Gameplay (Phase 3)
→ { "type": "player_state", "x", "y", "rot", "mode", ... }
← { "type": "start_level", "levelId", ... }
```

---

## Testing checklist per phase

### Phase 2 (security)

- [ ] Join without room code → rejected
- [ ] Join with wrong code → rejected
- [ ] Join with correct code → lobby works as today
- [ ] Friends-only on → non-friend rejected
- [ ] Friends-only off → room code alone is enough

### Phase 3 (avatars)

- [ ] Host + one client in same level
- [ ] Ghost visible with correct icon/colors
- [ ] Movement looks smooth (interpolation)
- [ ] Disconnect removes ghost
- [ ] Local gameplay unaffected (ghosts don't kill you)

---

## Related docs

- [DEV-SHARING.md](DEV-SHARING.md) — build and release dev builds
- [../changelog.md](../changelog.md) — shipped versions
- [Geode publishing](https://docs.geode-sdk.org/mods/publishing/) — Index when ready for public beta

---

## Release tagging reminder

When a phase ships to testers:

1. Bump `version` in `mod.json`
2. Update `changelog.md`
3. `git tag v<version> && git push origin v<version>`
4. GitHub Actions publishes `paxcirlot.clam.geode` to Releases
