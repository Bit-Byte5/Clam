#include "ClamPopup.hpp"

#include "../net/NetSession.hpp"

#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/TextInput.hpp>

using namespace geode::prelude;

namespace clam {

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
    if (!Popup::init(380.f, 280.f)) return false;

    setTitle("Clam");

    m_mainLayer->setLayout(AnchorLayout::create());

    m_statusLabel = CCLabelBMFont::create("Idle", "chatFont.fnt");
    m_statusLabel->setScale(0.45f);
    m_statusLabel->setAnchorPoint({0.f, 1.f});
    m_statusLabel->setID("status-label"_spr);
    m_mainLayer->addChildAtPosition(m_statusLabel, Anchor::Top, ccp(0.f, -36.f));

    m_hostInput = TextInput::create(180.f, "Host IP (e.g. 192.168.1.5)", "bigFont.fnt");
    m_hostInput->setFilter("0123456789.");
    m_hostInput->setString("127.0.0.1");
    m_hostInput->setID("host-input"_spr);
    m_mainLayer->addChildAtPosition(m_hostInput, Anchor::Top, ccp(0.f, -58.f));

    auto* buttonMenu = CCMenu::create();
    buttonMenu->setLayout(RowLayout::create()->setGap(6.f));
    m_mainLayer->addChildAtPosition(buttonMenu, Anchor::Top, ccp(0.f, -88.f));

    auto hostBtn = CCMenuItemExt::createSpriteExtra(
        ButtonSprite::create("Host", "goldFont.fnt", "GJ_button_01.png", .8f),
        [this](CCMenuItemSpriteExtra*) { onHost(nullptr); }
    );
    hostBtn->setID("host-btn"_spr);

    auto joinBtn = CCMenuItemExt::createSpriteExtra(
        ButtonSprite::create("Join", "goldFont.fnt", "GJ_button_01.png", .8f),
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
    m_mainLayer->addChildAtPosition(m_peerLabel, Anchor::Top, ccp(0.f, -118.f));

    auto* scroll = ScrollLayer::create({320.f, 110.f});
    scroll->setID("console-scroll"_spr);
    m_mainLayer->addChildAtPosition(scroll, Anchor::Center, ccp(0.f, -18.f));

    m_consoleLabel = CCLabelBMFont::create("", "chatFont.fnt");
    m_consoleLabel->setScale(0.38f);
    m_consoleLabel->setAnchorPoint({0.f, 1.f});
    m_consoleLabel->setAlignment(kCCTextAlignmentLeft);
    m_consoleLabel->setID("console-label"_spr);
    scroll->m_contentLayer->addChild(m_consoleLabel);
    scroll->m_contentLayer->setContentSize({320.f, 110.f});

    m_consoleText = "> Clam LAN console ready\n";
    m_consoleLabel->setString(m_consoleText.c_str());

    this->schedule(schedule_selector(ClamPopup::onTick), 0.25f);
    refreshUI();

    return true;
}

void ClamPopup::onClose(CCObject*) {
    this->unschedule(schedule_selector(ClamPopup::onTick));
    Popup::onClose(nullptr);
}

void ClamPopup::onHost(CCObject*) {
    auto* mod = Mod::get();
    int port = mod ? static_cast<int>(mod->getSettingValue<int64_t>("ws-port")) : 8765;

    if (NetSession::get().startHost(port, localPlayerName())) {
        refreshUI();
    }
}

void ClamPopup::onJoin(CCObject*) {
    auto* mod = Mod::get();
    int port = mod ? static_cast<int>(mod->getSettingValue<int64_t>("ws-port")) : 8765;
    auto host = m_hostInput ? m_hostInput->getString() : "127.0.0.1";

    if (host.empty()) {
        host = "127.0.0.1";
    }

    if (NetSession::get().join(host, port, localPlayerName())) {
        refreshUI();
    }
}

void ClamPopup::onStop(CCObject*) {
    NetSession::get().stop();
    refreshUI();
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
            scroll->m_contentLayer->setContentSize({320.f, std::max(110.f, size.height + 8.f)});
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
                    fmt::format("Hosting on port {}", session.port()).c_str()
                );
                break;
            case SessionRole::Client:
                m_statusLabel->setString(
                    fmt::format("Connected to {}:{}", session.hostAddress(), session.port()).c_str()
                );
                break;
            default:
                m_statusLabel->setString("Idle — Host or Join to start");
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

    if (!events.empty()) {
        refreshUI();
    }
}

} // namespace clam
