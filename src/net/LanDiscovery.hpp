#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace clam {

struct DiscoveredGame {
    std::string hostName;
    std::string hostAddress;
    int wsPort = 0;
    int players = 1;
    int64_t lastSeenMs = 0;
};

class LanDiscovery {
public:
    static LanDiscovery& get();

    void startBrowser(int discoveryPort);
    void stopBrowser();

    void startBroadcast(int discoveryPort, int wsPort, std::string const& hostName, std::function<int()> playerCount);
    void stopBroadcast();

    std::vector<DiscoveredGame> getGames() const;

private:
    LanDiscovery() = default;

    void pruneStaleLocked(int64_t nowMs);

    mutable std::mutex m_mutex;
    int m_discoveryPort = 8766;
    std::atomic<bool> m_listening{false};
    std::atomic<bool> m_broadcasting{false};
    std::thread m_listenThread;
    std::thread m_broadcastThread;

    int m_broadcastWsPort = 0;
    std::string m_broadcastHostName;
    std::function<int()> m_playerCount;

    std::vector<DiscoveredGame> m_games;
};

std::string displayPlayerName(bool shareUsername);

} // namespace clam
