#pragma once

#include <Geode/utils/general.hpp>

#include <cocos2d.h>

#include <cstdint>
#include <string>

namespace clam {

inline int packColor(cocos2d::ccColor3B const& color) {
    return (static_cast<int>(color.r) << 16)
        | (static_cast<int>(color.g) << 8)
        | static_cast<int>(color.b);
}

inline cocos2d::ccColor3B unpackColor(int packed) {
    return {
        static_cast<GLubyte>((packed >> 16) & 0xFF),
        static_cast<GLubyte>((packed >> 8) & 0xFF),
        static_cast<GLubyte>(packed & 0xFF),
    };
}

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
    int iconId,
    float scale,
    cocos2d::ccColor3B const& color1,
    cocos2d::ccColor3B const& color2
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
        {"scale", scale},
        {"color1", packColor(color1)},
        {"color2", packColor(color2)},
    }).dump();
}

inline bool isLobbyMessageType(std::string const& type) {
    return type == "hello" || type == "lobby";
}

} // namespace clam
