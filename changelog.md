# 1.1.6

- Faster ghost sync: default 30 Hz position updates (configurable 15–60 Hz)
- Timestamped snapshot interpolation with light extrapolation for smoother remote ghosts
- Lightweight position packets; full icon/color updates only when changed or every ~1s
- New settings: Position Send Rate and Ghost Interpolation Delay

# 1.1.5

- Ping/pong keepalive: detect lost host connection and auto-reconnect (up to 3 attempts)
- Clean disconnect if reconnect fails; UI shows "Reconnecting to host..."

# 1.1.4

- GameSync worker thread: JSON parse/serialize off main thread
- Lobby peer updates drained on main thread; cached nearby-player list

# 1.1.3

- Fix Mac crash when a client joins: stop restarting LAN broadcast on every lobby update (join/disconnect)

# 1.1.2

- Fix hang/crash when a client joins while the host is in a level (mutex deadlock in lobby updates)
- Process disconnects on the main thread instead of the WebSocket thread
- Keep LAN discovery running while the game is open (not only when the Clam popup is open)
- Linux: bind the broadcast socket so beacons route correctly to other machines

# 1.1.1

- Fix LAN discovery on Linux: broadcast to subnet addresses so Mac/Windows can find Linux hosts
- Clearer empty-state hint when no nearby players are in a level

# 1.1.0

- Ghost avatars: see connected peers on main campaign levels 1–22 (classic mode)
- Position sync over WebSocket at ~20 Hz (no collision or shared triggers)
- GameSync layer with PlayLayer hook and SimplePlayer ghosts
- LAN UI polish: tap-to-join nearby list (no IP), Host/Stop workflow
- Host appears on LAN only after entering a level; fixed nearby list discovery

# 1.0.1

- LAN discovery: nearby hosts show in menu with Join button
- WebSocket host/join lobby with peer list and console
- Mod settings: WebSocket port, discovery port, share username on LAN

# 1.0.0

- Initial Geode mod workspace setup
