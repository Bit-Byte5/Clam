#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "NetSession.hpp"

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

void NetSession::updateLanBroadcast() {
    SessionRole sessionRole;
    std::string hostName;
    int wsPort = 0;
    {
        std::lock_guard lock(m_mutex);
        sessionRole = m_role;
        hostName = m_localName;
        wsPort = m_port;
    }

    if (sessionRole != SessionRole::Host) {
        LanDiscovery::get().stopBroadcast();
        return;
    }

    LanDiscovery::get().startBroadcast(
        discoveryPortSetting(),
        wsPort,
        hostName,
        [this]() { return lobbyPlayerCountLocked(); }
    );
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
    if (role() != SessionRole::Host) {
        return games;
    }

    games.erase(
        std::remove_if(games.begin(), games.end(), [this](DiscoveredGame const& game) {
            return game.wsPort == port();
        }),
        games.end()
    );
    return games;
}

void NetSession::setPeers(std::vector<PeerInfo> peers) {
    std::lock_guard lock(m_mutex);
    m_peers = std::move(peers);
    updateLanBroadcast();
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

    setPeers(std::move(peers));
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
                session->setPeers(std::move(peers));
            }
        });

        host->server->set_message_handler([this, host](connection_hdl hdl, WSServer::message_ptr msg) {
            auto parsed = matjson::parse(msg->get_payload());
            if (!parsed) return;

            auto root = parsed.unwrap();
            if (!root.isObject()) return;
            if (root["type"].asString().unwrapOr("") != "hello") return;

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
            setPeers(std::move(peers));
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

        log("Hosting on port " + std::to_string(port));
        updateLanBroadcast();
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
            setPeers({});
        });

        client->client->set_message_handler([this](connection_hdl, WSClient::message_ptr msg) {
            handleLobbyMessage(msg->get_payload());
            log("<- lobby update (" + std::to_string(getPeers().size()) + " players)");
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
        m_peers.clear();
    }

    if (wasActive) {
        log("Session stopped");
    }
}

} // namespace clam
