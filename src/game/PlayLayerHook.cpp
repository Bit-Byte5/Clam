#include "GameSync.hpp"
#include "GhostManager.hpp"
#include "LevelGate.hpp"

#include "../net/NetSession.hpp"

#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

class $modify(ClamPlayLayer, PlayLayer) {
    struct Fields {
        bool clamActive = false;
        int levelId = 0;
        clam::GhostManager ghosts;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

        if (clam::isEligibleLevel(level, this)) {
            m_fields->clamActive = true;
            m_fields->levelId = clam::levelIdOf(level);
            clam::GameSync::get().onLevelEnter(m_fields->levelId);
        }

        return true;
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        if (!m_fields->clamActive) return;
        if (!isGameplayActive()) return;

        clam::NetSession::get().drainPendingUpdates();

        auto& sync = clam::GameSync::get();
        auto states = sync.getRemoteStates(m_fields->levelId);
        m_fields->ghosts.sync(this, states, dt);

        sync.tickLocal(this, dt);
    }

    void onQuit() {
        if (m_fields->clamActive) {
            clam::GameSync::get().onLevelExit();
            m_fields->ghosts.clear(this);
            m_fields->clamActive = false;
            m_fields->levelId = 0;
        }

        PlayLayer::onQuit();
    }

    void onExit() {
        if (m_fields->clamActive) {
            clam::GameSync::get().onLevelExit();
            m_fields->ghosts.clear(this);
            m_fields->clamActive = false;
            m_fields->levelId = 0;
        }

        PlayLayer::onExit();
    }
};
