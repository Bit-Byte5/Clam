#pragma once

#include "GameSync.hpp"

#include <Geode/binding/PlayLayer.hpp>
#include <Geode/binding/SimplePlayer.hpp>

#include <cstdint>
#include <unordered_map>

namespace clam {

class GhostManager {
public:
    void sync(PlayLayer* layer, std::vector<RemotePeerState> const& states, float dt);
    void clear(PlayLayer* layer);

private:
    struct GhostEntry {
        SimplePlayer* sprite = nullptr;
        int iconId = 0;
        float scale = 1.f;
        cocos2d::ccColor3B color1{255, 255, 255};
        cocos2d::ccColor3B color2{255, 255, 255};
        float x = 0.f;
        float y = 0.f;
        float rotation = 0.f;
        bool positioned = false;
    };

    std::unordered_map<uint64_t, GhostEntry> m_ghosts;

    void removeGhost(PlayLayer* layer, uint64_t peerId);
    void applyScale(GhostEntry& entry, float scale);
    void applyColors(GhostEntry& entry, cocos2d::ccColor3B const& color1, cocos2d::ccColor3B const& color2);
    void applyInterpolatedPosition(GhostEntry& entry, RemotePeerState const& state, float dt);
    SimplePlayer* ensureGhost(PlayLayer* layer, RemotePeerState const& state, GhostEntry*& outEntry);
};

} // namespace clam
