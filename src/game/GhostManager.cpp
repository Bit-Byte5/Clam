#include "GhostManager.hpp"

#include <cmath>

using namespace geode::prelude;

namespace clam {

namespace {

constexpr float kSmoothRate = 14.f;
constexpr float kSnapDistanceSq = 120.f * 120.f;

bool sameColor(cocos2d::ccColor3B const& a, cocos2d::ccColor3B const& b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

float lerpAngle(float from, float to, float t) {
    float diff = to - from;
    while (diff > 180.f) diff -= 360.f;
    while (diff < -180.f) diff += 360.f;
    return from + diff * t;
}

float smoothStep(float dt) {
    return 1.f - std::exp(-kSmoothRate * dt);
}

} // namespace

void GhostManager::applyColors(
    GhostEntry& entry,
    cocos2d::ccColor3B const& color1,
    cocos2d::ccColor3B const& color2
) {
    if (!entry.sprite) return;
    if (sameColor(entry.color1, color1) && sameColor(entry.color2, color2)) return;

    entry.color1 = color1;
    entry.color2 = color2;
    entry.sprite->setColors(color1, color2);
}

void GhostManager::applyScale(GhostEntry& entry, float scale) {
    if (scale <= 0.f) scale = 1.f;
    if (!entry.sprite) return;
    if (entry.scale == scale) return;

    entry.scale = scale;
    entry.sprite->setScale(scale);
}

void GhostManager::setTarget(GhostEntry& entry, RemotePeerState const& state) {
    entry.targetX = state.x;
    entry.targetY = state.y;
    entry.targetRotation = state.rotation;

    if (!entry.positioned) {
        entry.x = state.x;
        entry.y = state.y;
        entry.rotation = state.rotation;
        entry.positioned = true;
        return;
    }

    float dx = entry.targetX - entry.x;
    float dy = entry.targetY - entry.y;
    if (dx * dx + dy * dy >= kSnapDistanceSq) {
        entry.x = state.x;
        entry.y = state.y;
        entry.rotation = state.rotation;
    }
}

void GhostManager::applySmoothing(GhostEntry& entry, float dt) {
    if (!entry.positioned || !entry.sprite) return;

    float t = smoothStep(dt);
    entry.x += (entry.targetX - entry.x) * t;
    entry.y += (entry.targetY - entry.y) * t;
    entry.rotation = lerpAngle(entry.rotation, entry.targetRotation, t);

    entry.sprite->setPosition({entry.x, entry.y});
    entry.sprite->setRotation(entry.rotation);
}

void GhostManager::sync(PlayLayer* layer, std::vector<RemotePeerState> const& states, float dt) {
    if (!layer || !layer->m_objectLayer || !layer->m_player1) return;

    std::unordered_map<uint64_t, bool> seen;

    for (auto const& state : states) {
        seen[state.peerId] = true;

        if (state.dead) {
            if (auto it = m_ghosts.find(state.peerId); it != m_ghosts.end()) {
                if (it->second.sprite) {
                    it->second.sprite->setVisible(false);
                }
            }
            continue;
        }

        GhostEntry* entry = nullptr;
        auto* ghost = ensureGhost(layer, state, entry);
        if (!ghost || !entry) continue;

        ghost->setVisible(true);
        applyScale(*entry, state.scale);
        applyColors(*entry, state.color1, state.color2);
        setTarget(*entry, state);
        applySmoothing(*entry, dt);
    }

    std::vector<uint64_t> stale;
    for (auto const& [peerId, _] : m_ghosts) {
        if (!seen.contains(peerId)) {
            stale.push_back(peerId);
        }
    }

    for (auto peerId : stale) {
        removeGhost(layer, peerId);
    }
}

void GhostManager::clear(PlayLayer* layer) {
    for (auto const& [peerId, _] : m_ghosts) {
        removeGhost(layer, peerId);
    }
    m_ghosts.clear();
}

void GhostManager::removeGhost(PlayLayer* layer, uint64_t peerId) {
    auto it = m_ghosts.find(peerId);
    if (it == m_ghosts.end()) return;

    if (it->second.sprite) {
        it->second.sprite->removeFromParent();
    }

    m_ghosts.erase(it);
}

SimplePlayer* GhostManager::ensureGhost(
    PlayLayer* layer,
    RemotePeerState const& state,
    GhostEntry*& outEntry
) {
    auto it = m_ghosts.find(state.peerId);
    if (it != m_ghosts.end()) {
        if (it->second.iconId != state.iconId && it->second.sprite) {
            it->second.sprite->updatePlayerFrame(state.iconId, IconType::Cube);
            it->second.iconId = state.iconId;
        }
        outEntry = &it->second;
        return it->second.sprite;
    }

    auto* ghost = SimplePlayer::create(state.iconId);
    if (!ghost) return nullptr;

    ghost->setOpacity(180);
    ghost->setID(fmt::format("clam-ghost-{}", state.peerId).c_str());
    layer->m_objectLayer->addChild(ghost, 512);

    GhostEntry entry;
    entry.sprite = ghost;
    entry.iconId = state.iconId;
    entry.scale = state.scale > 0.f ? state.scale : 1.f;
    entry.color1 = state.color1;
    entry.color2 = state.color2;
    ghost->setScale(entry.scale);
    ghost->setColors(state.color1, state.color2);
    auto [inserted, _] = m_ghosts.emplace(state.peerId, std::move(entry));
    outEntry = &inserted->second;
    return ghost;
}

} // namespace clam
