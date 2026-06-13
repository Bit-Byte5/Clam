#pragma once

#include <Geode/utils/general.hpp>

#include <cstdint>
#include <string>

namespace clam {

inline std::string makeHello(std::string const& name) {
    return matjson::makeObject({
        {"type", "hello"},
        {"name", name},
    }).dump();
}

inline std::string makeLobby(matjson::Value const& players) {
    return matjson::makeObject({
        {"type", "lobby"},
        {"players", players},
    }).dump();
}

inline std::string makeLevelStart(uint64_t peerId, int levelId) {
    return matjson::makeObject({
        {"type", "level_start"},
        {"peerId", peerId},
        {"levelId", levelId},
    }).dump();
}

inline std::string makeLevelEnd(uint64_t peerId, int levelId) {
    return matjson::makeObject({
        {"type", "level_end"},
        {"peerId", peerId},
        {"levelId", levelId},
    }).dump();
}

inline std::string makePlayerState(
    uint64_t peerId,
    int levelId,
    float x,
    float y,
    float rotation,
    bool dead,
    int iconId
) {
    return matjson::makeObject({
        {"type", "player_state"},
        {"peerId", peerId},
        {"levelId", levelId},
        {"x", x},
        {"y", y},
        {"rotation", rotation},
        {"dead", dead},
        {"iconId", iconId},
    }).dump();
}

inline bool isLobbyMessageType(std::string const& type) {
    return type == "hello" || type == "lobby";
}

} // namespace clam
