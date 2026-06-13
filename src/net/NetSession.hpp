#pragma once

#include "LanDiscovery.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace clam {

enum class SessionRole {
    None,
    Host,
    Client,
};

struct NetEvent {
    std::string text;
};

struct PeerInfo {
    uint64_t id = 0;
    std::string name;
    bool local = false;
};

class NetSession {
public:
    static NetSession& get();

    bool startHost(int port, std::string const& playerName);
    bool join(std::string const& host, int port, std::string const& playerName);
    void stop();

    void tick(float dt);

    SessionRole role() const;
    bool isActive() const;
    bool isReconnecting() const;
    int port() const;
    int hostLevelId() const;
    std::string hostAddress() const;

    std::vector<NetEvent> drainEvents();
    std::vector<PeerInfo> getPeers() const;
    void drainPendingUpdates();
    std::vector<DiscoveredGame> getDiscoveredGames() const;

    void startLanBrowser();
    void stopLanBrowser();
    void invalidateNearbyCache();

    void hostEnteredLevel(int levelId);
    void hostLeftLevel();

    void sendGameMessage(std::string const& payload);
    uint64_t getLocalPeerId() const;

    void log(std::string text);

private:
    NetSession() = default;

    bool startClientTransport(std::string const& host, int port, std::string const& playerName);
    void stopClientTransport();
    void sendPing();
    void noteHostAlive();
    void onClientTransportOpen();
    void onClientTransportLost(bool unexpected);
    void beginReconnect();
    void tryReconnect();

    void setPeers(std::vector<PeerInfo> peers);
    void queueSetPeers(std::vector<PeerInfo> peers);
    void handleLobbyMessage(std::string const& payload);
    void updateLanBroadcast();
    int lobbyPlayerCountLocked() const;

    static int64_t nowMs();

    mutable std::mutex m_mutex;
    SessionRole m_role = SessionRole::None;
    int m_port = 0;
    std::string m_hostAddress;
    std::string m_localName;
    int m_hostLevelId = 0;
    std::vector<NetEvent> m_events;
    std::vector<PeerInfo> m_peers;
    std::optional<std::vector<PeerInfo>> m_pendingPeers;

    mutable std::mutex m_nearbyMutex;
    mutable std::vector<DiscoveredGame> m_nearbyCache;
    mutable std::string m_nearbyFingerprint;

    std::string m_reconnectHost;
    int m_reconnectPort = 0;

    std::atomic<int64_t> m_lastHostAliveMs{0};
    std::atomic<bool> m_clientConnected{false};
    std::atomic<bool> m_pendingReconnect{false};
    std::atomic<bool> m_reconnecting{false};
    std::atomic<bool> m_stopping{false};

    float m_pingTimer = 0.f;
    float m_reconnectDelay = 0.f;
    int m_reconnectAttempts = 0;
    uint64_t m_pingSeq = 0;

    static constexpr float kPingInterval = 3.f;
    static constexpr float kPongTimeout = 9.f;
    static constexpr float kReconnectDelay = 2.f;
    static constexpr int kMaxReconnectAttempts = 3;
};

std::string localPlayerName();
int wsPortSetting();
int discoveryPortSetting();
bool shareUsernameSetting();
int syncSendHzSetting();
int interpolationDelayMsSetting();

} // namespace clam
