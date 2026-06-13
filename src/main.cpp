#include "net/NetSession.hpp"
#include "ui/ClamPopup.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>

#include <cstdlib>

using namespace geode::prelude;

$on_mod(Loaded) {
    log::info("Clam loaded");
    clam::NetSession::get().startLanBrowser();
    std::atexit(+[]() {
        clam::NetSession::get().stop();
        clam::NetSession::get().stopLanBrowser();
    });
}

class $modify(ClamMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        auto* btn = CCMenuItemSpriteExtra::create(
            CCSprite::createWithSpriteFrameName("GJ_playGameBtn_001.png"),
            this,
            menu_selector(ClamMenuLayer::onClam)
        );
        btn->setScale(0.65f);
        btn->setID("clam-menu-btn"_spr);

        if (auto* menu = this->getChildByID("bottom-menu")) {
            menu->addChild(btn);
            menu->updateLayout();
        }

        return true;
    }

    void onClam(CCObject*) {
        clam::ClamPopup::create()->show();
    }
};
