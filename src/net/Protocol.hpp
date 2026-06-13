#pragma once

#include <Geode/utils/general.hpp>

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

} // namespace clam
