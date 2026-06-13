#pragma once

#include "GameSync.hpp"

#include <Geode/binding/PlayLayer.hpp>
#include <Geode/binding/SimplePlayer.hpp>

#include <cstdint>
#include <unordered_map>

namespace clam {

class GhostManager {
public:
    void sync(PlayLayer* layer, std::vector<RemotePeerState> const& states);
    void clear(PlayLayer* layer);

private:
    struct GhostEntry {
        SimplePlayer* sprite = nullptr;
        int iconId = 0;
    };

    std::unordered_map<uint64_t, GhostEntry> m_ghosts;

    void removeGhost(PlayLayer* layer, uint64_t peerId);
    SimplePlayer* ensureGhost(PlayLayer* layer, RemotePeerState const& state);
};

} // namespace clam
