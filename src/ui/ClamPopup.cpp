#include "ClamPopup.hpp"

#include "../net/NetSession.hpp"

#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/TextInput.hpp>

using namespace geode::prelude;

namespace clam {

namespace {

std::string fingerprintGames(std::vector<DiscoveredGame> const& games) {
    std::string out;
    for (auto const& game : games) {
        out += game.hostAddress + ":" + std::to_string(game.wsPort) + "|";
        out += game.hostName + "|";
        out += std::to_string(game.players) + ";";
    }
    return out;
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
    if (!Popup::init(380.f, 310.f)) return false;

    setTitle("Clam");

    m_mainLayer->setLayout(AnchorLayout::create());

    m_statusLabel = CCLabelBMFont::create("Idle", "chatFont.fnt");
    m_statusLabel->setScale(0.45f);
    m_statusLabel->setAnchorPoint({0.f, 1.f});
    m_statusLabel->setID("status-label"_spr);
    m_mainLayer->addChildAtPosition(m_statusLabel, Anchor::Top, ccp(0.f, -36.f));

    auto* nearbyLabel = CCLabelBMFont::create("Nearby on LAN", "chatFont.fnt");
    nearbyLabel->setScale(0.42f);
    nearbyLabel->setAnchorPoint({0.f, 1.f});
    nearbyLabel->setID("nearby-label"_spr);
    m_mainLayer->addChildAtPosition(nearbyLabel, Anchor::Top, ccp(-150.f, -56.f));

    m_nearbyScroll = ScrollLayer::create({300.f, 52.f});
    m_nearbyScroll->setID("nearby-scroll"_spr);
    m_mainLayer->addChildAtPosition(m_nearbyScroll, Anchor::Top, ccp(0.f, -78.f));

    m_hostInput = TextInput::create(180.f, "Host IP (manual)", "bigFont.fnt");
    m_hostInput->setFilter("0123456789.");
    m_hostInput->setString("127.0.0.1");
    m_hostInput->setID("host-input"_spr);
    m_mainLayer->addChildAtPosition(m_hostInput, Anchor::Top, ccp(0.f, -112.f));

    auto* buttonMenu = CCMenu::create();
    buttonMenu->setLayout(RowLayout::create()->setGap(6.f));
    m_mainLayer->addChildAtPosition(buttonMenu, Anchor::Top, ccp(0.f, -142.f));

    auto hostBtn = CCMenuItemExt::createSpriteExtra(
        ButtonSprite::create("Host", "goldFont.fnt", "GJ_button_01.png", .8f),
        [this](CCMenuItemSpriteExtra*) { onHost(nullptr); }
    );
    hostBtn->setID("host-btn"_spr);

    auto joinBtn = CCMenuItemExt::createSpriteExtra(
        ButtonSprite::create("Join IP", "goldFont.fnt", "GJ_button_01.png", .8f),
        [this](CCMenuItemSpriteExtra*) { onJoin(nullptr); }
    );
    joinBtn->setID("join-btn"_spr);

    auto stopBtn = CCMenuItemExt::createSpriteExtra(
        ButtonSprite::create("Stop", "goldFont.fnt", "GJ_button_04.png", .8f),
        [this](CCMenuItemSpriteExtra*) { onStop(nullptr); }
    );
    stopBtn->setID("stop-btn"_spr);

    buttonMenu->addChild(hostBtn);
    buttonMenu->addChild(joinBtn);
    buttonMenu->addChild(stopBtn);
    buttonMenu->updateLayout();

    m_peerLabel = CCLabelBMFont::create("Peers: (none)", "chatFont.fnt");
    m_peerLabel->setScale(0.42f);
    m_peerLabel->setAnchorPoint({0.f, 1.f});
    m_peerLabel->setID("peer-label"_spr);
    m_mainLayer->addChildAtPosition(m_peerLabel, Anchor::Top, ccp(0.f, -172.f));

    auto* scroll = ScrollLayer::create({320.f, 88.f});
    scroll->setID("console-scroll"_spr);
    m_mainLayer->addChildAtPosition(scroll, Anchor::Center, ccp(0.f, -24.f));

    m_consoleLabel = CCLabelBMFont::create("", "chatFont.fnt");
    m_consoleLabel->setScale(0.38f);
    m_consoleLabel->setAnchorPoint({0.f, 1.f});
    m_consoleLabel->setAlignment(kCCTextAlignmentLeft);
    m_consoleLabel->setID("console-label"_spr);
    scroll->m_contentLayer->addChild(m_consoleLabel);
    scroll->m_contentLayer->setContentSize({320.f, 88.f});

    m_consoleText = "> Clam LAN console ready\n";
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
        refreshUI();
        rebuildNearbyGames();
    }
}

void ClamPopup::onJoin(CCObject*) {
    auto host = m_hostInput ? m_hostInput->getString() : "127.0.0.1";
    if (host.empty()) {
        host = "127.0.0.1";
    }

    if (NetSession::get().join(host, wsPortSetting(), localPlayerName())) {
        refreshUI();
    }
}

void ClamPopup::joinDiscovered(DiscoveredGame const& game) {
    if (m_hostInput) {
        m_hostInput->setString(game.hostAddress);
    }

    if (NetSession::get().join(game.hostAddress, game.wsPort, localPlayerName())) {
        appendConsole("Joining " + game.hostName + " @ " + game.hostAddress);
        refreshUI();
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
        auto* label = CCLabelBMFont::create("Scanning LAN...", "chatFont.fnt");
        label->setScale(0.36f);
        label->setAnchorPoint({0.f, 0.5f});
        label->setID("nearby-empty"_spr);
        m_nearbyScroll->m_contentLayer->addChildAtPosition(label, Anchor::Left, ccp(4.f, 0.f));
        m_nearbyScroll->m_contentLayer->setContentSize({300.f, 52.f});
        return;
    }

    auto* layout = ColumnLayout::create()->setGap(3.f);
    m_nearbyScroll->m_contentLayer->setLayout(layout);

    for (auto const& game : games) {
        auto* row = CCMenu::create();
        row->setContentSize({290.f, 18.f});
        row->setLayout(RowLayout::create()->setGap(6.f));

        auto* text = CCLabelBMFont::create(
            fmt::format("{}  {}  ({}p)", game.hostName, game.hostAddress, game.players).c_str(),
            "chatFont.fnt"
        );
        text->setScale(0.34f);
        text->setAnchorPoint({0.f, 0.5f});

        auto* joinBtn = CCMenuItemExt::createSpriteExtra(
            ButtonSprite::create("Join", "goldFont.fnt", "GJ_button_01.png", .5f),
            [this, game](CCMenuItemSpriteExtra*) { joinDiscovered(game); }
        );

        row->addChild(text);
        row->addChild(joinBtn);
        row->updateLayout();
        m_nearbyScroll->m_contentLayer->addChild(row);
    }

    m_nearbyScroll->m_contentLayer->updateLayout();
    auto height = std::max(52.f, m_nearbyScroll->m_contentLayer->getContentHeight() + 4.f);
    m_nearbyScroll->m_contentLayer->setContentSize({300.f, height});
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
            scroll->m_contentLayer->setContentSize({320.f, std::max(88.f, size.height + 8.f)});
            scroll->scrollToTop();
        }
    }
}

void ClamPopup::refreshUI() {
    auto& session = NetSession::get();

    if (m_statusLabel) {
        switch (session.role()) {
            case SessionRole::Host:
                m_statusLabel->setString(
                    fmt::format("Hosting on port {} (visible on LAN)", session.port()).c_str()
                );
                break;
            case SessionRole::Client:
                m_statusLabel->setString(
                    fmt::format("Connected to {}:{}", session.hostAddress(), session.port()).c_str()
                );
                break;
            default:
                m_statusLabel->setString("Idle — host or join a nearby game");
                break;
        }
    }

    if (m_peerLabel) {
        auto peers = session.getPeers();
        if (peers.empty()) {
            m_peerLabel->setString("Peers: (none)");
        } else {
            std::string text = "Peers: ";
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

    if (!events.empty()) {
        refreshUI();
    }
}

} // namespace clam
