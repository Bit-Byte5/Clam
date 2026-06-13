#pragma once

#include "LanDiscovery.hpp"

#include <cstdint>
#include <mutex>
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

    SessionRole role() const;
    bool isActive() const;
    int port() const;
    int hostLevelId() const;
    std::string hostAddress() const;

    std::vector<NetEvent> drainEvents();
    std::vector<PeerInfo> getPeers() const;
    std::vector<DiscoveredGame> getDiscoveredGames() const;

    void startLanBrowser();
    void stopLanBrowser();

    void hostEnteredLevel(int levelId);
    void hostLeftLevel();

    void sendGameMessage(std::string const& payload);
    uint64_t getLocalPeerId() const;

    void log(std::string text);

private:
    NetSession() = default;

    void setPeers(std::vector<PeerInfo> peers);
    void handleLobbyMessage(std::string const& payload);
    void updateLanBroadcast();
    int lobbyPlayerCountLocked() const;

    mutable std::mutex m_mutex;
    SessionRole m_role = SessionRole::None;
    int m_port = 0;
    std::string m_hostAddress;
    std::string m_localName;
    int m_hostLevelId = 0;
    std::vector<NetEvent> m_events;
    std::vector<PeerInfo> m_peers;
};

std::string localPlayerName();
int wsPortSetting();
int discoveryPortSetting();
bool shareUsernameSetting();

} // namespace clam
