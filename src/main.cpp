#include "net/NetSession.hpp"
#include "ui/ClamPopup.hpp"

#include "game/GameSync.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>

#include <cstdlib>

using namespace geode::prelude;

$on_mod(Loaded) {
    log::info("Clam loaded");
    clam::NetSession::get().startLanBrowser();
    std::atexit(+[]() {
        clam::GameSync::get().shutdown();
        clam::NetSession::get().stop();
        clam::NetSession::get().stopLanBrowser();
    });
}

class $modify(ClamMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        auto* icon = CCSprite::createWithSpriteFrameName("clam_menu_icon.png"_spr);
        if (!icon) {
            icon = CCSprite::createWithSpriteFrameName("GJ_playGameBtn_001.png");
        }

        auto* btn = CCMenuItemSpriteExtra::create(
            icon,
            this,
            menu_selector(ClamMenuLayer::onClam)
        );
        btn->setScale(0.4f);
        btn->setID("clam-menu-btn"_spr);

        if (auto* menu = this->getChildByID("bottom-menu")) {
            menu->addChild(btn);
            menu->updateLayout();
        }

        this->schedule(schedule_selector(ClamMenuLayer::onClamTick), 0.25f);

        return true;
    }

    void onClamTick(float dt) {
        clam::NetSession::get().tick(dt);
    }

    void onClam(CCObject*) {
        clam::ClamPopup::create()->show();
    }
};
