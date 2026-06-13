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
