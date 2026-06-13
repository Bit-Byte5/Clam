#include "ClamPopup.hpp"

#include "../net/NetSession.hpp"

#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/ui/ScrollLayer.hpp>

using namespace geode::prelude;

namespace clam {

namespace {

std::string fingerprintGames(std::vector<DiscoveredGame> const& games) {
    std::string out;
    for (auto const& game : games) {
        out += game.hostAddress + ":" + std::to_string(game.wsPort) + "|";
        out += game.hostName + "|";
        out += std::to_string(game.players) + "|";
        out += std::to_string(game.levelId) + ";";
    }
    return out;
}

std::string levelLabel(int levelId) {
    if (levelId <= 0) return "In menu";
    return fmt::format("Level {}", levelId);
}

} // namespace

ClamPopup* ClamPopup::create() {
    auto* ret = new ClamPopup();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool ClamPopup::init() {
    if (!Popup::init(380.f, 300.f)) return false;

    setTitle("Clam");

    m_mainLayer->setLayout(AnchorLayout::create());

    m_statusLabel = CCLabelBMFont::create("Tap Host, then play a level", "chatFont.fnt");
    m_statusLabel->setScale(0.42f);
    m_statusLabel->setAnchorPoint({0.5f, 1.f});
    m_statusLabel->setID("status-label"_spr);
    m_mainLayer->addChildAtPosition(m_statusLabel, Anchor::Top, ccp(0.f, -36.f));

    auto* buttonMenu = CCMenu::create();
    buttonMenu->setLayout(RowLayout::create()->setGap(8.f));
    m_mainLayer->addChildAtPosition(buttonMenu, Anchor::Top, ccp(0.f, -68.f));

    auto hostBtn = CCMenuItemExt::createSpriteExtra(
        ButtonSprite::create("Host", "goldFont.fnt", "GJ_button_01.png", .8f),
        [this](CCMenuItemSpriteExtra*) { onHost(nullptr); }
    );
    hostBtn->setID("host-btn"_spr);

    auto stopBtn = CCMenuItemExt::createSpriteExtra(
        ButtonSprite::create("Stop", "goldFont.fnt", "GJ_button_04.png", .8f),
        [this](CCMenuItemSpriteExtra*) { onStop(nullptr); }
    );
    stopBtn->setID("stop-btn"_spr);

    buttonMenu->addChild(hostBtn);
    buttonMenu->addChild(stopBtn);
    buttonMenu->updateLayout();

    m_peerLabel = CCLabelBMFont::create("", "chatFont.fnt");
    m_peerLabel->setScale(0.4f);
    m_peerLabel->setAnchorPoint({0.5f, 1.f});
    m_peerLabel->setID("peer-label"_spr);
    m_mainLayer->addChildAtPosition(m_peerLabel, Anchor::Top, ccp(0.f, -96.f));

    auto* nearbyLabel = CCLabelBMFont::create("Nearby players", "chatFont.fnt");
    nearbyLabel->setScale(0.42f);
    nearbyLabel->setAnchorPoint({0.f, 1.f});
    nearbyLabel->setID("nearby-label"_spr);
    m_mainLayer->addChildAtPosition(nearbyLabel, Anchor::Top, ccp(-150.f, -118.f));

    m_nearbyScroll = ScrollLayer::create({320.f, 78.f});
    m_nearbyScroll->setID("nearby-scroll"_spr);
    m_mainLayer->addChildAtPosition(m_nearbyScroll, Anchor::Top, ccp(0.f, -132.f));

    auto* scroll = ScrollLayer::create({320.f, 72.f});
    scroll->setID("console-scroll"_spr);
    m_mainLayer->addChildAtPosition(scroll, Anchor::Bottom, ccp(0.f, 28.f));

    m_consoleLabel = CCLabelBMFont::create("", "chatFont.fnt");
    m_consoleLabel->setScale(0.36f);
    m_consoleLabel->setAnchorPoint({0.f, 1.f});
    m_consoleLabel->setAlignment(kCCTextAlignmentLeft);
    m_consoleLabel->setID("console-label"_spr);
    scroll->m_contentLayer->addChild(m_consoleLabel);
    scroll->m_contentLayer->setContentSize({320.f, 72.f});

    m_consoleText = "> Clam ready\n";
    m_consoleLabel->setString(m_consoleText.c_str());

    NetSession::get().startLanBrowser();
    rebuildNearbyGames();
    this->schedule(schedule_selector(ClamPopup::onTick), 0.25f);
    refreshUI();

    return true;
}

void ClamPopup::onClose(CCObject*) {
    this->unschedule(schedule_selector(ClamPopup::onTick));
    NetSession::get().stopLanBrowser();
    Popup::onClose(nullptr);
}

void ClamPopup::onHost(CCObject*) {
    if (NetSession::get().startHost(wsPortSetting(), localPlayerName())) {
        appendConsole("Hosting — enter a level to go live");
        refreshUI();
        rebuildNearbyGames();
    }
}

void ClamPopup::joinDiscovered(DiscoveredGame const& game) {
    if (NetSession::get().join(game.hostAddress, game.wsPort, localPlayerName())) {
        appendConsole("Joining " + game.hostName);
        refreshUI();
        rebuildNearbyGames();
    }
}

void ClamPopup::onStop(CCObject*) {
    NetSession::get().stop();
    refreshUI();
    rebuildNearbyGames();
}

void ClamPopup::rebuildNearbyGames() {
    if (!m_nearbyScroll) return;

    auto games = NetSession::get().getDiscoveredGames();
    auto fingerprint = fingerprintGames(games);
    if (fingerprint == m_nearbyFingerprint) return;
    m_nearbyFingerprint = std::move(fingerprint);

    m_nearbyScroll->m_contentLayer->removeAllChildren();
    m_nearbyScroll->m_contentLayer->setLayout(nullptr);

    if (games.empty()) {
        auto* label = CCLabelBMFont::create(
            "No players in a level nearby",
            "chatFont.fnt"
        );
        label->setScale(0.36f);
        label->setAnchorPoint({0.f, 0.5f});
        label->setID("nearby-empty"_spr);
        m_nearbyScroll->m_contentLayer->addChildAtPosition(label, Anchor::Left, ccp(4.f, 0.f));
        m_nearbyScroll->m_contentLayer->setContentSize({320.f, 78.f});
        return;
    }

    auto* layout = ColumnLayout::create()->setGap(4.f);
    m_nearbyScroll->m_contentLayer->setLayout(layout);

    for (auto const& game : games) {
        auto label = fmt::format(
            "{}   {}   ({}p)",
            game.hostName,
            levelLabel(game.levelId),
            game.players
        );

        auto* row = CCMenu::create();
        row->setContentSize({310.f, 22.f});

        auto* joinBtn = CCMenuItemExt::createSpriteExtra(
            ButtonSprite::create(label.c_str(), "goldFont.fnt", "GJ_button_01.png", .55f),
            [this, game](CCMenuItemSpriteExtra*) { joinDiscovered(game); }
        );
        joinBtn->setID(fmt::format("join-{}-{}", game.hostAddress, game.wsPort).c_str());

        row->addChild(joinBtn);
        row->updateLayout();
        m_nearbyScroll->m_contentLayer->addChild(row);
    }

    m_nearbyScroll->m_contentLayer->updateLayout();
    auto height = std::max(78.f, m_nearbyScroll->m_contentLayer->getContentHeight() + 4.f);
    m_nearbyScroll->m_contentLayer->setContentSize({320.f, height});
    m_nearbyScroll->scrollToTop();
}

void ClamPopup::appendConsole(std::string const& line) {
    m_consoleText += "> " + line + "\n";

    size_t start = 0;
    size_t lines = 0;
    for (size_t i = 0; i < m_consoleText.size(); ++i) {
        if (m_consoleText[i] == '\n') {
            lines++;
            if (lines > m_maxConsoleLines) {
                start = i + 1;
            }
        }
    }

    if (start > 0) {
        m_consoleText = m_consoleText.substr(start);
    }

    if (m_consoleLabel) {
        m_consoleLabel->setString(m_consoleText.c_str());
        auto size = m_consoleLabel->getContentSize();
        if (auto* scroll = typeinfo_cast<ScrollLayer*>(m_mainLayer->getChildByID("console-scroll"_spr))) {
            scroll->m_contentLayer->setContentSize({320.f, std::max(72.f, size.height + 8.f)});
            scroll->scrollToTop();
        }
    }
}

void ClamPopup::refreshUI() {
    auto& session = NetSession::get();

    if (m_statusLabel) {
        switch (session.role()) {
            case SessionRole::Host: {
                auto levelId = session.hostLevelId();
                if (levelId > 0) {
                    m_statusLabel->setString(
                        fmt::format("Live on LAN — {}", levelLabel(levelId)).c_str()
                    );
                } else {
                    m_statusLabel->setString("Hosting — enter a level to go live");
                }
                break;
            }
            case SessionRole::Client:
                m_statusLabel->setString("Connected — play the same level");
                break;
            default:
                m_statusLabel->setString("Tap Host, then play a level");
                break;
        }
    }

    if (m_peerLabel) {
        auto peers = session.getPeers();
        if (peers.empty() || session.role() == SessionRole::None) {
            m_peerLabel->setString("");
        } else {
            std::string text = "With: ";
            for (size_t i = 0; i < peers.size(); ++i) {
                if (i > 0) text += ", ";
                text += peers[i].name;
                if (peers[i].local) text += " (you)";
            }
            m_peerLabel->setString(text.c_str());
        }
    }
}

void ClamPopup::onTick(float) {
    auto events = NetSession::get().drainEvents();
    for (auto const& event : events) {
        appendConsole(event.text);
    }

    rebuildNearbyGames();
    refreshUI();
}

} // namespace clam
