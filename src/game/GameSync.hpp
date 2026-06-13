#pragma once

#include <Geode/binding/PlayLayer.hpp>

#include <cstdint>
#include <mutex>
#include <string>
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
    bool dead = false;
    int iconId = 1;
    bool inLevel = false;
    double lastSeenMs = 0.0;
};

class GameSync {
public:
    static GameSync& get();

    void queueIncoming(std::string const& payload);
    void drainIncoming();

    void onLevelEnter(int levelId);
    void onLevelExit();
    void onSessionStop();

    void tickLocal(PlayLayer* layer, float dt);

    std::vector<RemotePeerState> getRemoteStates(int levelId) const;
    void removePeer(uint64_t peerId);

private:
    GameSync() = default;

    void handleMessage(std::string const& payload);
    void applyPlayerState(matjson::Value const& root);
    std::string peerName(uint64_t peerId) const;
    int localIconId() const;

    mutable std::mutex m_mutex;
    std::vector<std::string> m_inbound;
    std::unordered_map<uint64_t, RemotePeerState> m_remotes;
    int m_localLevelId = 0;
    bool m_inLevel = false;
    float m_sendTimer = 0.f;
};

} // namespace clam
