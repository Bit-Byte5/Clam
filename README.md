# Clam

Geode mod for playing Geometry Dash with friends over LAN.

See each other's ghost avatars in main campaign levels 1–22 while everyone runs their own gameplay locally.

| | |
|---|---|
| **Mod ID** | `paxcirlot.clam` |
| **Latest release** | v1.1.3 |
| **GD / Geode** | 2.2081 / 5.7.1 |
| **Default ports** | WebSocket `8765`, discovery UDP `8766` |
| **Repo** | https://github.com/Bit-Byte5/Clam |

---

## How to play

1. Install Clam from [GitHub Releases](https://github.com/Bit-Byte5/Clam/releases) or Mocha Modding.
2. Open Geometry Dash → main menu → **Clam** button (play icon).
3. **Host** on one machine, then **enter a main level (1–22)**.
4. On other machines on the same Wi‑Fi, open Clam and tap the host in **Nearby players**.
5. Everyone loads the same level — you see each other as ghost cubes moving in real time.

**Requirements:** same Wi‑Fi, same Clam version, host must be inside a level to appear nearby. Host firewall may need TCP 8765 and UDP 8766 open.

---

## What works today

- LAN discovery without typing IP addresses
- WebSocket host/join with peer list and event console
- Ghost avatars with position sync (~20 Hz) on main campaign 1–22
- Host visible on LAN only while in an eligible level
- Cross-platform builds (Windows, macOS, Linux, Android, iOS)
- Mod settings: ports, share username on LAN

**Not yet:** room codes, friends-only gate, gamemode sync, shared gameplay, internet play.

---

## Development

```bash
export GEODE_SDK=/Users/Shared/Geode/sdk   # adjust to your SDK path
geode build
```

Opens GD → Clam button → Host or Join to test locally.

---

## Documentation

| Doc | Contents |
|-----|----------|
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | **System design** — threads, protocol, ghosts, source map |
| [docs/NEXT.md](docs/NEXT.md) | Roadmap — security, polish, future features |
| [docs/DEV-SHARING.md](docs/DEV-SHARING.md) | Build, install, and share dev `.geode` builds |
| [changelog.md](changelog.md) | Version history |
| [about.md](about.md) | In-game mod description |

---

## Sharing dev builds

1. Bump `version` in `mod.json`
2. `git tag v<version> && git push origin v<version>`
3. GitHub Actions uploads `paxcirlot.clam.geode` to Releases

Details: [docs/DEV-SHARING.md](docs/DEV-SHARING.md).
