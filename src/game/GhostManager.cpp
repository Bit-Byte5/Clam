#include "GhostManager.hpp"

#include "../net/NetSession.hpp"

#include <cmath>
#include <chrono>

using namespace geode::prelude;

namespace clam {

namespace {

constexpr float kSmoothRate = 22.f;
constexpr float kSnapDistanceSq = 160.f * 160.f;
constexpr float kMaxExtrapolation = 1.15f;

double nowMs() {
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

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

void interpolateSnapshot(
    RemotePeerState const& state,
    float& outX,
    float& outY,
    float& outRotation
) {
    outX = state.x;
    outY = state.y;
    outRotation = state.rotation;

    if (state.snapshotMs <= state.prevSnapshotMs) {
        return;
    }

    double const renderTime = nowMs() - static_cast<double>(interpolationDelayMsSetting());
    double alpha = (renderTime - state.prevSnapshotMs)
        / (state.snapshotMs - state.prevSnapshotMs);
    alpha = std::clamp(alpha, 0.0, static_cast<double>(kMaxExtrapolation));

    outX = static_cast<float>(state.prevX + (state.x - state.prevX) * alpha);
    outY = static_cast<float>(state.prevY + (state.y - state.prevY) * alpha);
    outRotation = lerpAngle(state.prevRotation, state.rotation, static_cast<float>(alpha));
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

void GhostManager::applyInterpolatedPosition(
    GhostEntry& entry,
    RemotePeerState const& state,
    float dt
) {
    if (!entry.sprite) return;

    float targetX = 0.f;
    float targetY = 0.f;
    float targetRotation = 0.f;
    interpolateSnapshot(state, targetX, targetY, targetRotation);

    if (!entry.positioned) {
        entry.x = targetX;
        entry.y = targetY;
        entry.rotation = targetRotation;
        entry.positioned = true;
    } else {
        float dx = targetX - entry.x;
        float dy = targetY - entry.y;
        if (dx * dx + dy * dy >= kSnapDistanceSq) {
            entry.x = targetX;
            entry.y = targetY;
            entry.rotation = targetRotation;
        } else {
            float t = smoothStep(dt);
            entry.x += (targetX - entry.x) * t;
            entry.y += (targetY - entry.y) * t;
            entry.rotation = lerpAngle(entry.rotation, targetRotation, t);
        }
    }

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
        applyInterpolatedPosition(*entry, state, dt);
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
