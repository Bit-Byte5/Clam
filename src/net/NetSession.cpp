#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "NetSession.hpp"

#include "../game/GameSync.hpp"
#include "LanDiscovery.hpp"
#include "Protocol.hpp"

#include <Geode/Geode.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/server.hpp>

#include <algorithm>
#include <atomic>
#include <map>
#include <memory>
#include <thread>

using namespace geode::prelude;

namespace clam {

namespace {

using WSServer = websocketpp::server<websocketpp::config::asio>;
using WSClient = websocketpp::client<websocketpp::config::asio_client>;
using connection_hdl = websocketpp::connection_hdl;

struct HostState {
    std::unique_ptr<WSServer> server;
    std::thread thread;
    std::atomic<bool> running{false};
    std::mutex sendMutex;
    std::map<connection_hdl, PeerInfo, std::owner_less<connection_hdl>> peers;
    uint64_t nextPeerId = 1;
};

struct ClientState {
    std::unique_ptr<WSClient> client;
    std::thread thread;
    WSClient::connection_ptr connection;
    std::atomic<bool> running{false};
    std::mutex sendMutex;
};

HostState* g_host = nullptr;
ClientState* g_client = nullptr;

void pushLobbyLocked(HostState* host) {
    auto players = matjson::Value::array();
    for (auto const& [_, peer] : host->peers) {
        players.push(matjson::makeObject({
            {"id", peer.id},
            {"name", peer.name},
        }));
    }

    auto payload = makeLobby(players);
    std::lock_guard sendLock(host->sendMutex);

    for (auto const& [hdl, _] : host->peers) {
        try {
            if (!hdl.expired()) {
                host->server->send(hdl, payload, websocketpp::frame::opcode::text);
            }
        } catch (...) {}
    }
}

bool sameConnection(connection_hdl a, connection_hdl b) {
    auto ownerLess = std::owner_less<connection_hdl>{};
    return !ownerLess(a, b) && !ownerLess(b, a);
}

void relayGameMessageLocked(HostState* host, connection_hdl sender, std::string const& payload) {
    std::lock_guard sendLock(host->sendMutex);
    for (auto const& [hdl, _] : host->peers) {
        if (sameConnection(hdl, sender)) continue;
        try {
            if (!hdl.expired()) {
                host->server->send(hdl, payload, websocketpp::frame::opcode::text);
            }
        } catch (...) {}
    }
}

void broadcastGameMessageLocked(HostState* host, std::string const& payload) {
    std::lock_guard sendLock(host->sendMutex);
    for (auto const& [hdl, _] : host->peers) {
        try {
            if (!hdl.expired()) {
                host->server->send(hdl, payload, websocketpp::frame::opcode::text);
            }
        } catch (...) {}
    }
}

} // namespace

NetSession& NetSession::get() {
    static NetSession session;
    return session;
}

std::string localPlayerName() {
    return displayPlayerName(shareUsernameSetting());
}

int wsPortSetting() {
    auto* mod = Mod::get();
    if (!mod) return 8765;
    return static_cast<int>(mod->getSettingValue<int64_t>("ws-port"));
}

int discoveryPortSetting() {
    auto* mod = Mod::get();
    if (!mod) return 8766;
    return static_cast<int>(mod->getSettingValue<int64_t>("discovery-port"));
}

bool shareUsernameSetting() {
    auto* mod = Mod::get();
    if (!mod) return true;
    return mod->getSettingValue<bool>("share-username");
}

SessionRole NetSession::role() const {
    std::lock_guard lock(m_mutex);
    return m_role;
}

bool NetSession::isActive() const {
    std::lock_guard lock(m_mutex);
    return m_role != SessionRole::None;
}

int NetSession::port() const {
    std::lock_guard lock(m_mutex);
    return m_port;
}

int NetSession::hostLevelId() const {
    std::lock_guard lock(m_mutex);
    return m_hostLevelId;
}

std::string NetSession::hostAddress() const {
    std::lock_guard lock(m_mutex);
    return m_hostAddress;
}

void NetSession::log(std::string text) {
    std::lock_guard lock(m_mutex);
    m_events.push_back({std::move(text)});
    log::info("[Clam] {}", m_events.back().text);
}

std::vector<NetEvent> NetSession::drainEvents() {
    std::lock_guard lock(m_mutex);
    auto out = std::move(m_events);
    m_events.clear();
    return out;
}

std::vector<PeerInfo> NetSession::getPeers() const {
    std::lock_guard lock(m_mutex);
    return m_peers;
}

uint64_t NetSession::getLocalPeerId() const {
    std::lock_guard lock(m_mutex);
    if (m_role == SessionRole::Host) return 0;
    for (auto const& peer : m_peers) {
        if (peer.local) return peer.id;
    }
    return 0;
}

void NetSession::sendGameMessage(std::string const& payload) {
    if (g_client) {
        auto* client = g_client;
        try {
            std::lock_guard lock(client->sendMutex);
            if (client->connection) {
                client->client->send(
                    client->connection->get_handle(),
                    payload,
                    websocketpp::frame::opcode::text
                );
            }
        } catch (...) {}
        return;
    }

    if (g_host) {
        broadcastGameMessageLocked(g_host, payload);
    }
}

void NetSession::updateLanBroadcast() {
    SessionRole sessionRole;
    std::string hostName;
    int wsPort = 0;
    int levelId = 0;
    {
        std::lock_guard lock(m_mutex);
        sessionRole = m_role;
        hostName = m_localName;
        wsPort = m_port;
        levelId = m_hostLevelId;
    }

    if (sessionRole != SessionRole::Host || levelId <= 0) {
        LanDiscovery::get().stopBroadcast();
        return;
    }

    LanDiscovery::get().startBroadcast(
        discoveryPortSetting(),
        wsPort,
        hostName,
        [this]() { return lobbyPlayerCountLocked(); },
        levelId
    );
}

void NetSession::hostEnteredLevel(int levelId) {
    {
        std::lock_guard lock(m_mutex);
        if (m_role != SessionRole::Host) return;
        m_hostLevelId = levelId;
    }
    log("Now visible on LAN (level " + std::to_string(levelId) + ")");
    updateLanBroadcast();
}

void NetSession::hostLeftLevel() {
    {
        std::lock_guard lock(m_mutex);
        if (m_role != SessionRole::Host) return;
        m_hostLevelId = 0;
    }
    LanDiscovery::get().stopBroadcast();
}

int NetSession::lobbyPlayerCountLocked() const {
    std::lock_guard lock(m_mutex);
    return static_cast<int>(m_peers.size());
}

void NetSession::startLanBrowser() {
    LanDiscovery::get().startBrowser(discoveryPortSetting());
}

void NetSession::stopLanBrowser() {
    LanDiscovery::get().stopBroadcast();
    LanDiscovery::get().stopBrowser();
}

std::vector<DiscoveredGame> NetSession::getDiscoveredGames() const {
    auto games = LanDiscovery::get().getGames();

    SessionRole sessionRole;
    int localPort = 0;
    std::string connectedHost;
    {
        std::lock_guard lock(m_mutex);
        sessionRole = m_role;
        localPort = m_port;
        connectedHost = m_hostAddress;
    }

    std::string fingerprint;
    fingerprint.reserve(games.size() * 48 + 32);
    for (auto const& game : games) {
        fingerprint += game.hostAddress;
        fingerprint += ':';
        fingerprint += std::to_string(game.wsPort);
        fingerprint += '|';
        fingerprint += std::to_string(game.levelId);
        fingerprint += '|';
        fingerprint += std::to_string(game.players);
        fingerprint += ';';
    }
    fingerprint += std::to_string(static_cast<int>(sessionRole));
    fingerprint += ':';
    fingerprint += std::to_string(localPort);
    fingerprint += ':';
    fingerprint += connectedHost;

    {
        std::lock_guard lock(m_nearbyMutex);
        if (fingerprint == m_nearbyFingerprint) {
            return m_nearbyCache;
        }
    }

    games.erase(
        std::remove_if(games.begin(), games.end(), [](DiscoveredGame const& game) {
            return game.levelId <= 0;
        }),
        games.end()
    );

    if (sessionRole == SessionRole::Host) {
        games.erase(
            std::remove_if(games.begin(), games.end(), [localPort](DiscoveredGame const& game) {
                return game.wsPort == localPort;
            }),
            games.end()
        );
    } else if (sessionRole == SessionRole::Client) {
        games.erase(
            std::remove_if(games.begin(), games.end(), [&](DiscoveredGame const& game) {
                return game.hostAddress == connectedHost && game.wsPort == localPort;
            }),
            games.end()
        );
    }

    {
        std::lock_guard lock(m_nearbyMutex);
        m_nearbyCache = games;
        m_nearbyFingerprint = std::move(fingerprint);
        return m_nearbyCache;
    }
}

void NetSession::setPeers(std::vector<PeerInfo> peers) {
    std::lock_guard lock(m_mutex);
    m_peers = std::move(peers);
}

void NetSession::queueSetPeers(std::vector<PeerInfo> peers) {
    std::lock_guard lock(m_mutex);
    m_pendingPeers = std::move(peers);
}

void NetSession::drainPendingUpdates() {
    std::optional<std::vector<PeerInfo>> peers;
    {
        std::lock_guard lock(m_mutex);
        peers = std::move(m_pendingPeers);
        m_pendingPeers.reset();
    }

    if (peers) {
        setPeers(std::move(*peers));
    }
}

void NetSession::handleLobbyMessage(std::string const& payload) {
    auto parsed = matjson::parse(payload);
    if (!parsed) return;

    auto root = parsed.unwrap();
    if (!root.isObject()) return;
    if (root["type"].asString().unwrapOr("") != "lobby") return;
    if (!root.contains("players") || !root["players"].isArray()) return;

    std::vector<PeerInfo> peers;
    for (auto const& entry : root["players"].asArray().unwrap()) {
        if (!entry.isObject()) continue;
        PeerInfo peer;
        peer.id = static_cast<uint64_t>(entry["id"].asInt().unwrapOr(0));
        peer.name = entry["name"].asString().unwrapOr("Unknown");
        peer.local = peer.name == m_localName;
        peers.push_back(std::move(peer));
    }

    queueSetPeers(std::move(peers));
}

bool NetSession::startHost(int port, std::string const& playerName) {
    stop();

    auto* host = new HostState();
    g_host = host;

    try {
        host->server = std::make_unique<WSServer>();
        host->server->set_access_channels(websocketpp::log::alevel::none);
        host->server->set_error_channels(websocketpp::log::elevel::none);
        host->server->init_asio();
        host->server->set_reuse_addr(true);

        host->server->set_open_handler([this, host](connection_hdl hdl) {
            PeerInfo peer;
            peer.id = host->nextPeerId++;
            peer.name = "Connecting...";

            {
                std::lock_guard lock(host->sendMutex);
                host->peers[hdl] = peer;
            }

            log("Client connected (#" + std::to_string(peer.id) + ")");
        });

        host->server->set_close_handler([this, host](connection_hdl hdl) {
            uint64_t id = 0;
            {
                std::lock_guard lock(host->sendMutex);
                if (auto it = host->peers.find(hdl); it != host->peers.end()) {
                    id = it->second.id;
                    host->peers.erase(it);
                }
            }

            if (id != 0) {
                log("Client disconnected (#" + std::to_string(id) + ")");
                GameSync::get().queueRemovePeer(id);
            }

            pushLobbyLocked(host);
            if (auto* session = &NetSession::get(); session->role() == SessionRole::Host) {
                std::vector<PeerInfo> peers;
                {
                    std::lock_guard lock(host->sendMutex);
                    for (auto const& [_, peer] : host->peers) {
                        peers.push_back(peer);
                    }
                }
                PeerInfo local;
                local.id = 0;
                local.name = session->m_localName;
                local.local = true;
                peers.insert(peers.begin(), local);
                session->queueSetPeers(std::move(peers));
            }
        });

        host->server->set_message_handler([this, host](connection_hdl hdl, WSServer::message_ptr msg) {
            auto payload = msg->get_payload();
            auto parsed = matjson::parse(payload);
            if (!parsed) return;

            auto root = parsed.unwrap();
            if (!root.isObject()) return;
            auto type = root["type"].asString().unwrapOr("");

            if (type == "hello") {
                auto name = root["name"].asString().unwrapOr("Unknown");
                {
                    std::lock_guard lock(host->sendMutex);
                    if (auto it = host->peers.find(hdl); it != host->peers.end()) {
                        it->second.name = name;
                    }
                }

                log("<- hello from " + name);
                pushLobbyLocked(host);

                std::vector<PeerInfo> peers;
                PeerInfo local;
                local.id = 0;
                local.name = m_localName;
                local.local = true;
                peers.push_back(local);

                {
                    std::lock_guard lock(host->sendMutex);
                    for (auto const& [_, peer] : host->peers) {
                        peers.push_back(peer);
                    }
                }
                queueSetPeers(std::move(peers));
                return;
            }

            if (isLobbyMessageType(type)) return;

            GameSync::get().queueIncoming(payload);
            relayGameMessageLocked(host, hdl, payload);
        });

        host->server->listen(
            websocketpp::lib::asio::ip::address::from_string("0.0.0.0"),
            static_cast<uint16_t>(port)
        );
        host->server->start_accept();
        host->running.store(true);

        host->thread = std::thread([host]() {
            try {
                host->server->run();
            } catch (std::exception const& e) {
                log::error("[Clam] Host thread error: {}", e.what());
            } catch (...) {
                log::error("[Clam] Host thread unknown error");
            }
            host->running.store(false);
        });

        {
            std::lock_guard lock(m_mutex);
            m_role = SessionRole::Host;
            m_port = port;
            m_hostAddress = "0.0.0.0";
            m_localName = playerName;
            m_peers = {{0, playerName, true}};
        }

        log("Hosting — enter a level to appear nearby");
        return true;
    } catch (std::exception const& e) {
        log("Failed to start host: " + std::string(e.what()));
        delete host;
        g_host = nullptr;
        return false;
    }
}

bool NetSession::join(std::string const& host, int port, std::string const& playerName) {
    stop();

    auto* client = new ClientState();
    g_client = client;

    try {
        client->client = std::make_unique<WSClient>();
        client->client->set_access_channels(websocketpp::log::alevel::none);
        client->client->set_error_channels(websocketpp::log::elevel::none);
        client->client->init_asio();

        auto uri = "ws://" + host + ":" + std::to_string(port);

        client->client->set_open_handler([this, client, playerName](connection_hdl) {
            log("Connected to host");
            try {
                std::lock_guard lock(client->sendMutex);
                client->client->send(
                    client->connection->get_handle(),
                    makeHello(playerName),
                    websocketpp::frame::opcode::text
                );
            } catch (...) {}
            log("-> hello as " + playerName);
        });

        client->client->set_close_handler([this](connection_hdl) {
            log("Disconnected from host");
            queueSetPeers({});
            GameSync::get().queueSessionStop();
        });

        client->client->set_message_handler([this](connection_hdl, WSClient::message_ptr msg) {
            auto payload = msg->get_payload();
            auto parsed = matjson::parse(payload);
            if (!parsed) return;

            auto root = parsed.unwrap();
            if (!root.isObject()) return;
            auto type = root["type"].asString().unwrapOr("");

            if (type == "lobby") {
                handleLobbyMessage(payload);
                log("<- lobby update");
                return;
            }

            if (isLobbyMessageType(type)) return;

            GameSync::get().queueIncoming(payload);
        });

        websocketpp::lib::error_code ec;
        client->connection = client->client->get_connection(uri, ec);
        if (ec) {
            log("Failed to connect: " + ec.message());
            delete client;
            g_client = nullptr;
            return false;
        }

        client->client->connect(client->connection);
        client->running.store(true);

        client->thread = std::thread([client]() {
            try {
                client->client->run();
            } catch (std::exception const& e) {
                log::error("[Clam] Client thread error: {}", e.what());
            } catch (...) {
                log::error("[Clam] Client thread unknown error");
            }
            client->running.store(false);
        });

        {
            std::lock_guard lock(m_mutex);
            m_role = SessionRole::Client;
            m_port = port;
            m_hostAddress = host;
            m_localName = playerName;
            m_peers = {{0, playerName, true}};
        }

        log("Joining " + uri + " ...");
        return true;
    } catch (std::exception const& e) {
        log("Failed to join: " + std::string(e.what()));
        delete client;
        g_client = nullptr;
        return false;
    }
}

void NetSession::stop() {
    bool wasActive = false;
    {
        std::lock_guard lock(m_mutex);
        wasActive = m_role != SessionRole::None;
    }

    if (g_client) {
        auto* client = g_client;
        g_client = nullptr;

        client->running.store(false);
        try {
            if (client->connection) {
                client->client->close(
                    client->connection->get_handle(),
                    websocketpp::close::status::going_away,
                    "Clam stopped"
                );
            }
            client->client->stop();
        } catch (...) {}

        if (client->thread.joinable()) {
            client->thread.join();
        }

        delete client;
    }

    if (g_host) {
        auto* host = g_host;
        g_host = nullptr;

        host->running.store(false);
        try {
            host->server->stop_listening();
            host->server->stop();
        } catch (...) {}

        if (host->thread.joinable()) {
            host->thread.join();
        }

        delete host;
    }

    LanDiscovery::get().stopBroadcast();

    {
        std::lock_guard lock(m_mutex);
        m_role = SessionRole::None;
        m_port = 0;
        m_hostAddress.clear();
        m_hostLevelId = 0;
        m_peers.clear();
        m_pendingPeers.reset();
    }

    {
        std::lock_guard lock(m_nearbyMutex);
        m_nearbyCache.clear();
        m_nearbyFingerprint.clear();
    }

    GameSync::get().onSessionStop();

    if (wasActive) {
        log("Session stopped");
    }
}

} // namespace clam
