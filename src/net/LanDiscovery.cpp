#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#if !defined(__ANDROID__)
#include <ifaddrs.h>
#include <net/if.h>
#endif
#endif

#include "LanDiscovery.hpp"

#include "Protocol.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/GJAccountManager.hpp>

#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

using namespace geode::prelude;

namespace clam {

namespace {

constexpr int kBeaconProtocol = 1;
constexpr int kStaleMs = 5000;
constexpr int kBroadcastIntervalMs = 1500;

int64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

#ifdef _WIN32
using socket_t = SOCKET;
constexpr socket_t kInvalidSocket = INVALID_SOCKET;

void closeSocket(socket_t sock) {
    if (sock != kInvalidSocket) closesocket(sock);
}

bool initSockets() {
    static std::once_flag flag;
    static bool ok = false;
    std::call_once(flag, []() {
        WSADATA data;
        ok = WSAStartup(MAKEWORD(2, 2), &data) == 0;
    });
    return ok;
}
#else
using socket_t = int;
constexpr socket_t kInvalidSocket = -1;

void closeSocket(socket_t sock) {
    if (sock >= 0) close(sock);
}

bool initSockets() {
    return true;
}
#endif

std::string makeBeacon(std::string const& hostName, int wsPort, int players, int levelId) {
    return matjson::makeObject({
        {"type", "clam_beacon"},
        {"protocol", kBeaconProtocol},
        {"hostName", hostName},
        {"wsPort", wsPort},
        {"players", players},
        {"levelId", levelId},
    }).dump();
}

bool parseBeacon(
    std::string const& payload,
    std::string& hostName,
    int& wsPort,
    int& players,
    int& levelId
) {
    auto parsed = matjson::parse(payload);
    if (!parsed) return false;

    auto root = parsed.unwrap();
    if (!root.isObject()) return false;
    if (root["type"].asString().unwrapOr("") != "clam_beacon") return false;
    if (root["protocol"].asInt().unwrapOr(0) != kBeaconProtocol) return false;

    hostName = root["hostName"].asString().unwrapOr("Player");
    wsPort = static_cast<int>(root["wsPort"].asInt().unwrapOr(0));
    players = static_cast<int>(root["players"].asInt().unwrapOr(1));
    levelId = static_cast<int>(root["levelId"].asInt().unwrapOr(0));
    return wsPort > 0;
}

std::vector<uint32_t> collectBroadcastTargets() {
    std::vector<uint32_t> targets;
    auto addTarget = [&](uint32_t addr) {
        if (addr == 0) return;
        for (auto existing : targets) {
            if (existing == addr) return;
        }
        targets.push_back(addr);
    };

    addTarget(htonl(INADDR_BROADCAST));

#if !defined(_WIN32) && !defined(__ANDROID__)
    ifaddrs* interfaces = nullptr;
    if (getifaddrs(&interfaces) == 0) {
        for (auto* iface = interfaces; iface != nullptr; iface = iface->ifa_next) {
            if (!iface->ifa_addr || iface->ifa_addr->sa_family != AF_INET) continue;
            if (!(iface->ifa_flags & IFF_UP)) continue;
            if (iface->ifa_flags & IFF_LOOPBACK) continue;
            if (!iface->ifa_netmask) continue;

            auto* addr = reinterpret_cast<sockaddr_in*>(iface->ifa_addr);
            auto* mask = reinterpret_cast<sockaddr_in*>(iface->ifa_netmask);
            addTarget(addr->sin_addr.s_addr | ~mask->sin_addr.s_addr);
        }
        freeifaddrs(interfaces);
    }
#endif

    return targets;
}

void sendBeaconToTargets(
    socket_t sock,
    int discoveryPort,
    std::string const& payload,
    std::vector<uint32_t> const& targets
) {
    for (auto target : targets) {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<uint16_t>(discoveryPort));
        addr.sin_addr.s_addr = target;

        auto sent = sendto(
            sock,
            payload.c_str(),
            static_cast<int>(payload.size()),
            0,
            reinterpret_cast<sockaddr*>(&addr),
            sizeof(addr)
        );

        if (sent < 0) {
            log::warn("[Clam] LAN beacon send failed for target {}", inet_ntoa(addr.sin_addr));
        }
    }
}

} // namespace

LanDiscovery& LanDiscovery::get() {
    static LanDiscovery discovery;
    return discovery;
}

std::string displayPlayerName(bool shareUsername) {
    if (!shareUsername) {
        return "Player";
    }
    if (auto* account = GJAccountManager::get()) {
        if (!account->m_username.empty()) {
            return account->m_username;
        }
    }
    return "Player";
}

void LanDiscovery::pruneStaleLocked(int64_t now) {
    m_games.erase(
        std::remove_if(m_games.begin(), m_games.end(), [&](DiscoveredGame const& game) {
            return now - game.lastSeenMs > kStaleMs;
        }),
        m_games.end()
    );
}

void LanDiscovery::startBrowser(int discoveryPort) {
    stopBrowser();

    if (!initSockets()) {
        log::error("[Clam] Failed to initialize sockets for LAN discovery");
        return;
    }

    m_discoveryPort = discoveryPort;
    m_listening.store(true);

    m_listenThread = std::thread([this]() {
        socket_t sock = kInvalidSocket;
        try {
            sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (sock == kInvalidSocket) return;

            int reuse = 1;
            setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char const*>(&reuse), sizeof(reuse));

            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
            addr.sin_port = htons(static_cast<uint16_t>(m_discoveryPort));

            if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
                log::error("[Clam] LAN discovery bind failed on port {}", m_discoveryPort);
                return;
            }

#ifdef _WIN32
            DWORD timeout = 1000;
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&timeout), sizeof(timeout));
#else
            timeval timeout{1, 0};
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif

            char buffer[512];
            while (m_listening.load()) {
                sockaddr_in from{};
                socklen_t fromLen = sizeof(from);
                auto received = recvfrom(
                    sock,
                    buffer,
                    sizeof(buffer) - 1,
                    0,
                    reinterpret_cast<sockaddr*>(&from),
                    &fromLen
                );

                if (received <= 0) {
                    auto now = nowMs();
                    std::lock_guard lock(m_mutex);
                    pruneStaleLocked(now);
                    continue;
                }

                buffer[received] = '\0';
                std::string hostName;
                int wsPort = 0;
                int players = 1;
                int levelId = 0;
                if (!parseBeacon(buffer, hostName, wsPort, players, levelId)) continue;

                char ipStr[INET_ADDRSTRLEN] = {};
                inet_ntop(AF_INET, &from.sin_addr, ipStr, sizeof(ipStr));
                auto hostAddress = std::string(ipStr);
                auto seen = nowMs();

                std::lock_guard lock(m_mutex);
                bool updated = false;
                for (auto& game : m_games) {
                    if (game.hostAddress == hostAddress && game.wsPort == wsPort) {
                        game.hostName = hostName;
                        game.players = players;
                        game.levelId = levelId;
                        game.lastSeenMs = seen;
                        updated = true;
                        break;
                    }
                }
                if (!updated) {
                    m_games.push_back({hostName, hostAddress, wsPort, players, levelId, seen});
                }
                pruneStaleLocked(seen);
            }
        } catch (std::exception const& e) {
            log::error("[Clam] LAN listen thread error: {}", e.what());
        } catch (...) {
            log::error("[Clam] LAN listen thread unknown error");
        }

        closeSocket(sock);
    });
}

void LanDiscovery::stopBrowser() {
    m_listening.store(false);
    if (m_listenThread.joinable()) {
        m_listenThread.join();
    }

    std::lock_guard lock(m_mutex);
    m_games.clear();
}

void LanDiscovery::startBroadcast(
    int discoveryPort,
    int wsPort,
    std::string const& hostName,
    std::function<int()> playerCount,
    int levelId
) {
    stopBroadcast();

    if (!initSockets()) return;

    m_discoveryPort = discoveryPort;
    m_broadcastWsPort = wsPort;
    m_broadcastLevelId = levelId;
    m_broadcastHostName = hostName;
    m_playerCount = std::move(playerCount);
    m_broadcasting.store(true);

    m_broadcastThread = std::thread([this]() {
        socket_t sock = kInvalidSocket;
        try {
            sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (sock == kInvalidSocket) return;

            int broadcastEnable = 1;
            setsockopt(sock, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<char*>(&broadcastEnable), sizeof(broadcastEnable));

            sockaddr_in bindAddr{};
            bindAddr.sin_family = AF_INET;
            bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
            bindAddr.sin_port = htons(0);
            if (bind(sock, reinterpret_cast<sockaddr*>(&bindAddr), sizeof(bindAddr)) != 0) {
                log::warn("[Clam] LAN broadcast bind failed — beacons may not leave this machine");
            }

            auto targets = collectBroadcastTargets();
            log::info("[Clam] LAN broadcast using {} target(s)", targets.size());

            while (m_broadcasting.load()) {
                int players = 1;
                if (m_playerCount) {
                    players = std::max(1, m_playerCount());
                }

                auto payload = makeBeacon(
                    m_broadcastHostName,
                    m_broadcastWsPort,
                    players,
                    m_broadcastLevelId
                );
                sendBeaconToTargets(sock, m_discoveryPort, payload, targets);

                std::this_thread::sleep_for(std::chrono::milliseconds(kBroadcastIntervalMs));
            }
        } catch (std::exception const& e) {
            log::error("[Clam] LAN broadcast thread error: {}", e.what());
        } catch (...) {
            log::error("[Clam] LAN broadcast thread unknown error");
        }

        closeSocket(sock);
    });
}

void LanDiscovery::stopBroadcast() {
    m_broadcasting.store(false);
    if (m_broadcastThread.joinable()) {
        m_broadcastThread.join();
    }
    m_playerCount = nullptr;
}

std::vector<DiscoveredGame> LanDiscovery::getGames() const {
    std::lock_guard lock(m_mutex);
    auto now = nowMs();
    auto games = m_games;
    games.erase(
        std::remove_if(games.begin(), games.end(), [&](DiscoveredGame const& game) {
            return now - game.lastSeenMs > kStaleMs;
        }),
        games.end()
    );
    return games;
}

} // namespace clam
