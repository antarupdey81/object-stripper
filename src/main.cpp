#include <Geode/Geode.hpp>
#include <Geode/modify/EditorPauseLayer.hpp>
#include <vector>
#include <string>

using namespace geode::prelude;

class StripObjectsPopup : public geode::Popup {
protected:
    EditorUI* m_editorUI = nullptr;
    TextInput* m_countInput = nullptr;
    CCLabelBMFont* m_currentCountLabel = nullptr;

    bool init(EditorUI* editorUI) {
        if (!Popup::init(260.f, 185.f)) {
            return false;
        }

        m_editorUI = editorUI;
        this->setTitle("Object Stripper");

        auto editorLayer = LevelEditorLayer::get();
        size_t currentCount = (editorLayer && editorLayer->m_objects)
            ? editorLayer->m_objects->count()
            : 0;

        m_currentCountLabel = CCLabelBMFont::create(
            fmt::format("Current Objects: {}", currentCount).c_str(),
            "goldFont.fnt"
        );
        m_currentCountLabel->setScale(0.55f);
        m_currentCountLabel->setPosition({ m_size.width / 2.f, m_size.height - 52.f });
        m_mainLayer->addChild(m_currentCountLabel);

        auto hintLabel = CCLabelBMFont::create("Target object count:", "bigFont.fnt");
        hintLabel->setScale(0.45f);
        hintLabel->setPosition({ m_size.width / 2.f, m_size.height - 80.f });
        m_mainLayer->addChild(hintLabel);

        m_countInput = TextInput::create(140.f, "e.g. 500", "bigFont.fnt");
        m_countInput->setFilter("0123456789");
        m_countInput->setMaxCharCount(7);
        m_countInput->setPosition({ m_size.width / 2.f, m_size.height - 108.f });
        m_mainLayer->addChild(m_countInput);

        auto stripSpr = ButtonSprite::create("Strip", "goldFont.fnt", "GJ_button_06.png", 0.8f);
        auto stripBtn = CCMenuItemSpriteExtra::create(
            stripSpr,
            this,
            menu_selector(StripObjectsPopup::onStrip)
        );
        stripBtn->setPosition({ m_size.width / 2.f, 34.f });
        m_buttonMenu->addChild(stripBtn);

        return true;
    }

    void onStrip(CCObject*) {
        auto editorLayer = LevelEditorLayer::get();
        if (!editorLayer || !editorLayer->m_objects) {
            FLAlertLayer::create("Error", "Could not access editor objects.", "OK")->show();
            return;
        }

        std::string text = m_countInput->getString();
        if (text.empty()) {
            FLAlertLayer::create("Invalid Input", "Please enter a target object count.", "OK")->show();
            return;
        }

        size_t targetCount = 0;
        try {
            targetCount = static_cast<size_t>(std::stoull(text));
        } catch (...) {
            FLAlertLayer::create("Invalid Input", "Number is too large or invalid.", "OK")->show();
            return;
        }

        size_t totalObjects = editorLayer->m_objects->count();
        if (targetCount >= totalObjects) {
            FLAlertLayer::create(
                "Nothing to Strip",
                fmt::format("Level already has {} objects (target: {}).", totalObjects, targetCount),
                "OK"
            )->show();
            return;
        }

        size_t toDelete = totalObjects - targetCount;

        // Collect objects from the end backwards, skipping locked layers
        std::vector<GameObject*> objectsToRemove;
        objectsToRemove.reserve(toDelete);

        for (int i = static_cast<int>(totalObjects) - 1; i >= 0 && objectsToRemove.size() < toDelete; --i) {
            auto obj = static_cast<GameObject*>(editorLayer->m_objects->objectAtIndex(i));
            if (!obj) continue;

            if (editorLayer->isLayerLocked(obj->m_editorLayer) ||
                (obj->m_editorLayer2 != 0 && editorLayer->isLayerLocked(obj->m_editorLayer2))) {
                continue;
            }

            objectsToRemove.push_back(obj);
        }

        size_t removedCount = objectsToRemove.size();

        if (m_editorUI) {
            m_editorUI->deselectAll();
        }

        for (auto* obj : objectsToRemove) {
            editorLayer->removeObject(obj, false);
        }

        if (m_editorUI) {
            m_editorUI->updateButtons();
            m_editorUI->updateObjectInfoLabel();
        }

        size_t remaining = editorLayer->m_objects->count();

        if (removedCount < toDelete) {
            FLAlertLayer::create(
                "Partially Stripped",
                fmt::format(
                    "Deleted <cy>{}</c> objects.\nStopped at <cg>{}</c> objects because remaining objects are on <cr>locked layers</c>.",
                    removedCount,
                    remaining
                ),
                "OK"
            )->show();
        } else {
            FLAlertLayer::create(
                "Objects Stripped",
                fmt::format(
                    "Deleted <cy>{}</c> objects.\nLevel now has <cg>{}</c> objects.",
                    removedCount,
                    remaining
                ),
                "OK"
            )->show();
        }

        this->onClose(nullptr);
    }

public:
    static StripObjectsPopup* create(EditorUI* editorUI) {
        auto ret = new StripObjectsPopup();
        if (ret && ret->init(editorUI)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

class $modify(StripEditorPauseLayer, EditorPauseLayer) {
    bool init(LevelEditorLayer* editorLayer) {
        if (!EditorPauseLayer::init(editorLayer)) {
            return false;
        }

        auto spr = ButtonSprite::create("Strip Objs", "bigFont.fnt", "GJ_button_06.png", 0.6f);
        spr->setScale(0.75f);

        auto btn = CCMenuItemSpriteExtra::create(
            spr,
            this,
            menu_selector(StripEditorPauseLayer::onOpenStripper)
        );
        btn->setID("strip-objects-btn"_spr);

        if (auto menu = this->getChildByID("small-actions-menu")) {
            menu->addChild(btn);
            menu->updateLayout();
        } else if (auto actionsMenu = this->getChildByID("actions-menu")) {
            actionsMenu->addChild(btn);
            actionsMenu->updateLayout();
        } else {
            auto winSize = CCDirector::sharedDirector()->getWinSize();
            auto fallbackMenu = CCMenu::create();
            fallbackMenu->setPosition({ 65.f, winSize.height - 90.f });
            fallbackMenu->addChild(btn);
            this->addChild(fallbackMenu);
        }

        return true;
    }

    void onOpenStripper(CCObject*) {
        auto editorLayer = LevelEditorLayer::get();
        EditorUI* ui = editorLayer ? editorLayer->m_editorUI : nullptr;
        StripObjectsPopup::create(ui)->show();
    }
};