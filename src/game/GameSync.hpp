#pragma once

#include <Geode/binding/PlayLayer.hpp>

#include <cocos2d.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace clam {

struct RemotePeerState {
    uint64_t peerId = 0;
    std::string name;
    int levelId = 0;
    float x = 0.f;
    float y = 0.f;
    float rotation = 0.f;
    float prevX = 0.f;
    float prevY = 0.f;
    float prevRotation = 0.f;
    bool dead = false;
    int iconId = 1;
    float scale = 1.f;
    cocos2d::ccColor3B color1{255, 255, 255};
    cocos2d::ccColor3B color2{255, 255, 255};
    bool inLevel = false;
    double lastSeenMs = 0.0;
    double snapshotMs = 0.0;
    double prevSnapshotMs = 0.0;
};

struct LocalPlayerSnapshot {
    uint64_t peerId = 0;
    int levelId = 0;
    float x = 0.f;
    float y = 0.f;
    float rotation = 0.f;
    bool dead = false;
    int iconId = 1;
    float scale = 1.f;
    cocos2d::ccColor3B color1{255, 255, 255};
    cocos2d::ccColor3B color2{255, 255, 255};
    int64_t snapshotMs = 0;
    bool forceFull = false;
};

class GameSync {
public:
    static GameSync& get();

    void shutdown();

    void queueIncoming(std::string const& payload);

    void onLevelEnter(int levelId);
    void onLevelExit();
    void onSessionStop();

    void tickLocal(PlayLayer* layer, float dt);

    std::vector<RemotePeerState> getRemoteStates(int levelId) const;
    void queueRemovePeer(uint64_t peerId);
    void queueSessionStop();

private:
    GameSync();
    ~GameSync();

    GameSync(GameSync const&) = delete;
    GameSync& operator=(GameSync const&) = delete;

    enum class CommandType {
        InboundJson,
        OutboundJson,
        RemovePeer,
        SessionStop,
    };

    struct Command {
        CommandType type = CommandType::InboundJson;
        std::string payload;
        uint64_t peerId = 0;
    };

    void startWorker();
    void workerLoop();
    void notifyWorker();
    bool hasWorkerWorkLocked() const;
    void processCommand(Command const& cmd);
    void handleMessage(std::string const& payload);
    void applyPlayerState(matjson::Value const& root);
    void sendPlayerState(LocalPlayerSnapshot const& snapshot);
    bool cosmeticsChanged(LocalPlayerSnapshot const& snapshot) const;
    std::string peerName(uint64_t peerId) const;
    int localIconId() const;
    void clearRemoteState();
    float sendIntervalSeconds() const;

    std::thread m_worker;
    std::atomic<bool> m_workerRunning{false};

    mutable std::mutex m_queueMutex;
    std::condition_variable m_queueCv;
    std::vector<Command> m_commands;
    std::optional<LocalPlayerSnapshot> m_pendingSnapshot;

    mutable std::mutex m_stateMutex;
    std::unordered_map<uint64_t, RemotePeerState> m_remotes;

    int m_localLevelId = 0;
    bool m_inLevel = false;
    float m_sendTimer = 0.f;
    float m_fullRefreshTimer = 0.f;
    bool m_hasLastSentSnapshot = false;
    LocalPlayerSnapshot m_lastSentSnapshot;
};

} // namespace clam
