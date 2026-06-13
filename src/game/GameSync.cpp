#include "GameSync.hpp"

#include "../net/NetSession.hpp"
#include "../net/Protocol.hpp"

#include <Geode/binding/GameManager.hpp>
#include <Geode/binding/PlayerObject.hpp>

#include <chrono>

using namespace geode::prelude;

namespace clam {

namespace {

double nowMs() {
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

} // namespace

GameSync& GameSync::get() {
    static GameSync sync;
    return sync;
}

void GameSync::queueIncoming(std::string const& payload) {
    std::lock_guard lock(m_mutex);
    m_inbound.push_back(payload);
}

void GameSync::drainIncoming() {
    std::vector<std::string> batch;
    {
        std::lock_guard lock(m_mutex);
        batch = std::move(m_inbound);
    }

    for (auto const& payload : batch) {
        handleMessage(payload);
    }
}

void GameSync::onLevelEnter(int levelId) {
    if (!NetSession::get().isActive()) return;

    m_localLevelId = levelId;
    m_inLevel = true;
    m_sendTimer = 0.f;

    if (NetSession::get().role() == SessionRole::Host) {
        NetSession::get().hostEnteredLevel(levelId);
    }

    auto peerId = NetSession::get().getLocalPeerId();
    NetSession::get().sendGameMessage(makeLevelStart(peerId, levelId));
}

void GameSync::onLevelExit() {
    if (!m_inLevel) return;

    if (NetSession::get().isActive()) {
        if (NetSession::get().role() == SessionRole::Host) {
            NetSession::get().hostLeftLevel();
        }

        auto peerId = NetSession::get().getLocalPeerId();
        NetSession::get().sendGameMessage(makeLevelEnd(peerId, m_localLevelId));
    }

    m_inLevel = false;
    m_localLevelId = 0;
    m_sendTimer = 0.f;

    std::lock_guard lock(m_mutex);
    m_remotes.clear();
}

void GameSync::onSessionStop() {
    m_inLevel = false;
    m_localLevelId = 0;
    m_sendTimer = 0.f;

    std::lock_guard lock(m_mutex);
    m_remotes.clear();
    m_inbound.clear();
}

void GameSync::tickLocal(PlayLayer* layer, float dt) {
    if (!m_inLevel || !layer || !layer->m_player1) return;
    if (!NetSession::get().isActive()) return;

    m_sendTimer += dt;
    if (m_sendTimer < 0.05f) return;
    m_sendTimer = 0.f;

    auto* player = layer->m_player1;
    auto pos = player->getPosition();

    auto peerId = NetSession::get().getLocalPeerId();
    NetSession::get().sendGameMessage(makePlayerState(
        peerId,
        m_localLevelId,
        pos.x,
        pos.y,
        player->getObjectRotation(),
        player->m_isDead,
        localIconId()
    ));
}

std::vector<RemotePeerState> GameSync::getRemoteStates(int levelId) const {
    std::lock_guard lock(m_mutex);
    std::vector<RemotePeerState> out;

    auto localPeerId = NetSession::get().getLocalPeerId();
    for (auto const& [peerId, state] : m_remotes) {
        if (peerId == localPeerId) continue;
        if (state.levelId != levelId) continue;
        if (!state.inLevel) continue;
        out.push_back(state);
    }

    return out;
}

void GameSync::removePeer(uint64_t peerId) {
    std::lock_guard lock(m_mutex);
    m_remotes.erase(peerId);
}

void GameSync::handleMessage(std::string const& payload) {
    auto parsed = matjson::parse(payload);
    if (!parsed) return;

    auto root = parsed.unwrap();
    if (!root.isObject()) return;

    auto type = root["type"].asString().unwrapOr("");
    auto peerId = static_cast<uint64_t>(root["peerId"].asInt().unwrapOr(0));
    if (peerId == NetSession::get().getLocalPeerId()) return;

    if (type == "level_start") {
        auto levelId = static_cast<int>(root["levelId"].asInt().unwrapOr(0));

        std::lock_guard lock(m_mutex);
        auto& state = m_remotes[peerId];
        state.peerId = peerId;
        state.name = peerName(peerId);
        state.levelId = levelId;
        state.inLevel = true;
        state.lastSeenMs = nowMs();
        return;
    }

    if (type == "level_end") {
        std::lock_guard lock(m_mutex);
        m_remotes.erase(peerId);
        return;
    }

    if (type == "player_state") {
        applyPlayerState(root);
    }
}

void GameSync::applyPlayerState(matjson::Value const& root) {
    auto peerId = static_cast<uint64_t>(root["peerId"].asInt().unwrapOr(0));

    std::lock_guard lock(m_mutex);
    auto& state = m_remotes[peerId];
    state.peerId = peerId;
    state.name = peerName(peerId);
    state.levelId = static_cast<int>(root["levelId"].asInt().unwrapOr(0));
    state.x = static_cast<float>(root["x"].asDouble().unwrapOr(0.0));
    state.y = static_cast<float>(root["y"].asDouble().unwrapOr(0.0));
    state.rotation = static_cast<float>(root["rotation"].asDouble().unwrapOr(0.0));
    state.dead = root["dead"].asBool().unwrapOr(false);
    state.iconId = static_cast<int>(root["iconId"].asInt().unwrapOr(1));
    state.inLevel = true;
    state.lastSeenMs = nowMs();
}

std::string GameSync::peerName(uint64_t peerId) const {
    for (auto const& peer : NetSession::get().getPeers()) {
        if (peer.id == peerId) return peer.name;
    }
    return "Player";
}

int GameSync::localIconId() const {
    auto* gm = GameManager::get();
    if (!gm) return 1;
    return static_cast<int>(gm->m_playerFrame);
}

} // namespace clam
