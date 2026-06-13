#include "GhostManager.hpp"

using namespace geode::prelude;

namespace clam {

void GhostManager::sync(PlayLayer* layer, std::vector<RemotePeerState> const& states) {
    if (!layer || !layer->m_objectLayer) return;

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

        auto* ghost = ensureGhost(layer, state);
        if (!ghost) continue;

        ghost->setVisible(true);
        ghost->setPosition({state.x, state.y});
        ghost->setRotation(state.rotation);
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

SimplePlayer* GhostManager::ensureGhost(PlayLayer* layer, RemotePeerState const& state) {
    auto it = m_ghosts.find(state.peerId);
    if (it != m_ghosts.end()) {
        if (it->second.iconId != state.iconId && it->second.sprite) {
            it->second.sprite->updatePlayerFrame(state.iconId, IconType::Cube);
            it->second.iconId = state.iconId;
        }
        return it->second.sprite;
    }

    auto* ghost = SimplePlayer::create(state.iconId);
    if (!ghost) return nullptr;

    ghost->setScale(0.5f);
    ghost->setOpacity(180);
    ghost->setID(fmt::format("clam-ghost-{}", state.peerId).c_str());
    layer->m_objectLayer->addChild(ghost, 512);

    GhostEntry entry;
    entry.sprite = ghost;
    entry.iconId = state.iconId;
    m_ghosts[state.peerId] = entry;

    return ghost;
}

} // namespace clam
