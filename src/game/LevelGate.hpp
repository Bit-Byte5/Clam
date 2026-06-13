#pragma once

#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/PlayLayer.hpp>

#include "../net/NetSession.hpp"

namespace clam {

inline int levelIdOf(GJGameLevel* level) {
    if (!level) return 0;
    return static_cast<int>(level->m_levelID);
}

inline bool isMainCampaignLevel(int levelId) {
    return levelId >= 1 && levelId <= 22;
}

inline bool isEligibleLevel(GJGameLevel* level, PlayLayer* playLayer) {
    if (!level || !playLayer) return false;
    if (!NetSession::get().isActive()) return false;

    int levelId = levelIdOf(level);
    if (level->m_levelType != GJLevelType::Main) return false;
    if (!isMainCampaignLevel(levelId)) return false;
    if (level->isPlatformer()) return false;
    if (playLayer->m_isPlatformer) return false;

    return true;
}

} // namespace clam
