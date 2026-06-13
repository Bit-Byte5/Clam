#pragma once

#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/TextInput.hpp>

#include "../net/LanDiscovery.hpp"

namespace clam {

class ClamPopup : public geode::Popup {
protected:
    bool init() override;
    void onClose(cocos2d::CCObject*) override;
    void onTick(float dt);

    void onHost(CCObject*);
    void onJoin(CCObject*);
    void onStop(CCObject*);
    void joinDiscovered(DiscoveredGame const& game);
    void refreshUI();
    void rebuildNearbyGames();
    void appendConsole(std::string const& line);

    cocos2d::CCLabelBMFont* m_statusLabel = nullptr;
    cocos2d::CCLabelBMFont* m_peerLabel = nullptr;
    cocos2d::CCLabelBMFont* m_consoleLabel = nullptr;
    geode::ScrollLayer* m_nearbyScroll = nullptr;
    geode::TextInput* m_hostInput = nullptr;

    std::string m_consoleText;
    std::string m_nearbyFingerprint;
    size_t m_maxConsoleLines = 10;

public:
    static ClamPopup* create();
};

} // namespace clam
