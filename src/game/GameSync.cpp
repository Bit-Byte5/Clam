#include "GameSync.hpp"

#include "../net/NetSession.hpp"
#include "../net/Protocol.hpp"

#include <Geode/binding/GameManager.hpp>
#include <Geode/binding/PlayerObject.hpp>

#include <chrono>
#include <utility>

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

GameSync::GameSync() {
    startWorker();
}

GameSync::~GameSync() {
    shutdown();
}

void GameSync::shutdown() {
    if (!m_workerRunning.exchange(false)) return;

    {
        std::lock_guard lock(m_queueMutex);
        m_queueCv.notify_all();
    }

    if (m_worker.joinable()) {
        m_worker.join();
    }
}

void GameSync::startWorker() {
    if (m_workerRunning.load()) return;
    m_workerRunning.store(true);
    m_worker = std::thread([this]() { workerLoop(); });
}

void GameSync::notifyWorker() {
    m_queueCv.notify_one();
}

bool GameSync::hasWorkerWorkLocked() const {
    return !m_commands.empty() || m_pendingSnapshot.has_value();
}

void GameSync::workerLoop() {
    while (m_workerRunning.load()) {
        std::vector<Command> commands;
        std::optional<LocalPlayerSnapshot> snapshot;

        {
            std::unique_lock lock(m_queueMutex);
            m_queueCv.wait(lock, [this]() {
                return !m_workerRunning.load() || hasWorkerWorkLocked();
            });

            if (!m_workerRunning.load() && !hasWorkerWorkLocked()) {
                break;
            }

            commands = std::move(m_commands);
            snapshot = std::move(m_pendingSnapshot);
        }

        for (auto const& command : commands) {
            processCommand(command);
        }

        if (snapshot) {
            sendPlayerState(*snapshot);
        }
    }
}

void GameSync::processCommand(Command const& cmd) {
    switch (cmd.type) {
        case CommandType::InboundJson:
            handleMessage(cmd.payload);
            break;
        case CommandType::OutboundJson:
            NetSession::get().sendGameMessage(cmd.payload);
            break;
        case CommandType::RemovePeer: {
            std::lock_guard lock(m_stateMutex);
            m_remotes.erase(cmd.peerId);
            break;
        }
        case CommandType::SessionStop:
            clearRemoteState();
            break;
    }
}

void GameSync::queueIncoming(std::string const& payload) {
    {
        std::lock_guard lock(m_queueMutex);
        m_commands.push_back({CommandType::InboundJson, payload});
    }
    notifyWorker();
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
    {
        std::lock_guard lock(m_queueMutex);
        m_commands.push_back({
            CommandType::OutboundJson,
            makeLevelStart(peerId, levelId),
        });
    }
    notifyWorker();
}

void GameSync::onLevelExit() {
    if (!m_inLevel) return;

    if (NetSession::get().isActive()) {
        if (NetSession::get().role() == SessionRole::Host) {
            NetSession::get().hostLeftLevel();
        }

        auto peerId = NetSession::get().getLocalPeerId();
        {
            std::lock_guard lock(m_queueMutex);
            m_commands.push_back({
                CommandType::OutboundJson,
                makeLevelEnd(peerId, m_localLevelId),
            });
        }
        notifyWorker();
    }

    m_inLevel = false;
    m_localLevelId = 0;
    m_sendTimer = 0.f;
    clearRemoteState();
}

void GameSync::onSessionStop() {
    m_inLevel = false;
    m_localLevelId = 0;
    m_sendTimer = 0.f;
    clearRemoteState();

    {
        std::lock_guard lock(m_queueMutex);
        m_commands.clear();
        m_pendingSnapshot.reset();
    }
}

void GameSync::clearRemoteState() {
    std::lock_guard lock(m_stateMutex);
    m_remotes.clear();
}

void GameSync::tickLocal(PlayLayer* layer, float dt) {
    if (!m_inLevel || !layer || !layer->m_player1) return;
    if (!NetSession::get().isActive()) return;

    m_sendTimer += dt;
    if (m_sendTimer < 0.05f) return;
    m_sendTimer = 0.f;

    auto* player = layer->m_player1;
    auto pos = player->getPosition();

    LocalPlayerSnapshot snapshot;
    snapshot.peerId = NetSession::get().getLocalPeerId();
    snapshot.levelId = m_localLevelId;
    snapshot.x = pos.x;
    snapshot.y = pos.y;
    snapshot.rotation = player->getObjectRotation();
    snapshot.dead = player->m_isDead;
    snapshot.iconId = localIconId();
    snapshot.scale = player->getScale();
    if (snapshot.scale <= 0.f) snapshot.scale = 1.f;
    snapshot.color1 = player->m_playerColor1;
    snapshot.color2 = player->m_playerColor2;

    {
        std::lock_guard lock(m_queueMutex);
        m_pendingSnapshot = snapshot;
    }
    notifyWorker();
}

void GameSync::sendPlayerState(LocalPlayerSnapshot const& snapshot) {
    NetSession::get().sendGameMessage(makePlayerState(
        snapshot.peerId,
        snapshot.levelId,
        snapshot.x,
        snapshot.y,
        snapshot.rotation,
        snapshot.dead,
        snapshot.iconId,
        snapshot.scale,
        snapshot.color1,
        snapshot.color2
    ));
}

std::vector<RemotePeerState> GameSync::getRemoteStates(int levelId) const {
    std::lock_guard lock(m_stateMutex);
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

void GameSync::queueRemovePeer(uint64_t peerId) {
    {
        std::lock_guard lock(m_queueMutex);
        m_commands.push_back({CommandType::RemovePeer, {}, peerId});
    }
    notifyWorker();
}

void GameSync::queueSessionStop() {
    {
        std::lock_guard lock(m_queueMutex);
        m_commands.push_back({CommandType::SessionStop});
    }
    notifyWorker();
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

        std::lock_guard lock(m_stateMutex);
        auto& state = m_remotes[peerId];
        state.peerId = peerId;
        state.name = peerName(peerId);
        state.levelId = levelId;
        state.inLevel = true;
        state.lastSeenMs = nowMs();
        return;
    }

    if (type == "level_end") {
        std::lock_guard lock(m_stateMutex);
        m_remotes.erase(peerId);
        return;
    }

    if (type == "player_state") {
        applyPlayerState(root);
    }
}

void GameSync::applyPlayerState(matjson::Value const& root) {
    auto peerId = static_cast<uint64_t>(root["peerId"].asInt().unwrapOr(0));

    std::lock_guard lock(m_stateMutex);
    auto& state = m_remotes[peerId];
    state.peerId = peerId;
    state.name = peerName(peerId);
    state.levelId = static_cast<int>(root["levelId"].asInt().unwrapOr(0));
    state.x = static_cast<float>(root["x"].asDouble().unwrapOr(0.0));
    state.y = static_cast<float>(root["y"].asDouble().unwrapOr(0.0));
    state.rotation = static_cast<float>(root["rotation"].asDouble().unwrapOr(0.0));
    state.dead = root["dead"].asBool().unwrapOr(false);
    state.iconId = static_cast<int>(root["iconId"].asInt().unwrapOr(1));
    state.scale = static_cast<float>(root["scale"].asDouble().unwrapOr(1.0));
    if (state.scale <= 0.f) state.scale = 1.f;
    state.color1 = unpackColor(static_cast<int>(root["color1"].asInt().unwrapOr(0xFFFFFF)));
    state.color2 = unpackColor(static_cast<int>(root["color2"].asInt().unwrapOr(0xFFFFFF)));
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
