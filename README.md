# Clam

Geode mod for playing Geometry Dash with friends over LAN.

**Mod ID:** `paxcirlot.clam` · **Default port:** `8765` · **GD 2.2081** · **Geode 5.7.1**

## Quick start (development)

```bash
export GEODE_SDK=/Users/Shared/Geode/sdk
geode build
```

Open GD → main menu → Clam button → **Host** or **Join** to test LAN WebSocket sessions.

## Documentation

| Doc | Contents |
|-----|----------|
| [docs/DEV-SHARING.md](docs/DEV-SHARING.md) | Build, install, and share dev `.geode` builds with friends |
| [about.md](about.md) | In-game mod description |
| [changelog.md](changelog.md) | Version history |

## Sharing dev builds

See **[docs/DEV-SHARING.md](docs/DEV-SHARING.md)** for the full workflow. Short version:

1. Bump `version` in `mod.json`
2. `git tag v<version> && git push origin v<version>`
3. GitHub Actions uploads `paxcirlot.clam.geode` to Releases
4. Friends install via Geode mods menu (folder button or install from file)
