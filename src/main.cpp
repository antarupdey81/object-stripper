#include <Geode/Geode.hpp>
#include <Geode/modify/EditorPauseLayer.hpp>
#include <vector>
#include <string>

using namespace geode::prelude;

static bool isGameplayObject(GameObject* obj) {
    if (!obj) return false;

    // In GD, purely decorative objects use GameObjectType::Decoration (7).
    // Everything else (Solid, Hazard, Slope, Portals, Pads, Orbs, Coins, Breakables, etc.)
    // plus triggers and Start Pos (ID 31) is gameplay/functional.
    if (obj->m_objectType != GameObjectType::Decoration) {
        return true;
    }

    if (obj->m_objectID == 31 || obj->m_isTrigger) {
        return true;
    }

    return false;
}

class StripObjectsPopup : public geode::Popup {
protected:
    EditorUI* m_editorUI = nullptr;
    TextInput* m_countInput = nullptr;
    CCLabelBMFont* m_currentCountLabel = nullptr;
    CCMenuItemToggler* m_decoOnlyToggle = nullptr;
    bool m_decoOnly = false;

    bool init(EditorUI* editorUI) {
        if (!Popup::init(275.f, 215.f)) {
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
        m_currentCountLabel->setPosition({ m_size.width / 2.f, m_size.height - 50.f });
        m_mainLayer->addChild(m_currentCountLabel);

        auto hintLabel = CCLabelBMFont::create("Target object count:", "bigFont.fnt");
        hintLabel->setScale(0.45f);
        hintLabel->setPosition({ m_size.width / 2.f, m_size.height - 76.f });
        m_mainLayer->addChild(hintLabel);

        m_countInput = TextInput::create(145.f, "e.g. 500", "bigFont.fnt");
        m_countInput->setFilter("0123456789");
        m_countInput->setMaxCharCount(7);
        m_countInput->setPosition({ m_size.width / 2.f, m_size.height - 104.f });
        m_mainLayer->addChild(m_countInput);

        // Strip Deco Only toggle row
        m_decoOnlyToggle = CCMenuItemToggler::createWithStandardSprites(
            this,
            menu_selector(StripObjectsPopup::onToggleDecoOnly),
            0.65f
        );
        m_decoOnlyToggle->setPosition({ m_size.width / 2.f - 62.f, m_size.height - 140.f });
        m_buttonMenu->addChild(m_decoOnlyToggle);

        auto decoLabel = CCLabelBMFont::create("Strip Deco Only", "bigFont.fnt");
        decoLabel->setScale(0.42f);
        decoLabel->setAnchorPoint({ 0.f, 0.5f });
        decoLabel->setPosition({ m_size.width / 2.f - 44.f, m_size.height - 140.f });
        m_mainLayer->addChild(decoLabel);

        auto stripSpr = ButtonSprite::create("Strip", "goldFont.fnt", "GJ_button_06.png", 0.8f);
        auto stripBtn = CCMenuItemSpriteExtra::create(
            stripSpr,
            this,
            menu_selector(StripObjectsPopup::onStrip)
        );
        stripBtn->setPosition({ m_size.width / 2.f, 32.f });
        m_buttonMenu->addChild(stripBtn);

        return true;
    }

    void onToggleDecoOnly(CCObject*) {
        m_decoOnly = !m_decoOnly;
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

        std::vector<GameObject*> objectsToRemove;
        objectsToRemove.reserve(toDelete);

        for (int i = static_cast<int>(totalObjects) - 1; i >= 0 && objectsToRemove.size() < toDelete; --i) {
            auto obj = static_cast<GameObject*>(editorLayer->m_objects->objectAtIndex(i));
            if (!obj) continue;

            // Always protect locked layers
            if (editorLayer->isLayerLocked(obj->m_editorLayer) ||
                (obj->m_editorLayer2 != 0 && editorLayer->isLayerLocked(obj->m_editorLayer2))) {
                continue;
            }

            // If Strip Deco Only is enabled, protect gameplay objects and triggers
            if (m_decoOnly && isGameplayObject(obj)) {
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
            std::string reason = m_decoOnly
                ? "remaining objects are <cy>gameplay</c> or on <cr>locked layers</c>."
                : "remaining objects are on <cr>locked layers</c>.";
            FLAlertLayer::create(
                "Partially Stripped",
                fmt::format(
                    "Deleted <cy>{}</c> objects.\nStopped at <cg>{}</c> objects because {}",
                    removedCount,
                    remaining,
                    reason
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
        spr->setScale(0.7f);

        auto btn = CCMenuItemSpriteExtra::create(
            spr,
            this,
            menu_selector(StripEditorPauseLayer::onOpenStripper)
        );
        btn->setID("strip-objects-btn"_spr);

        // Place in guidelines-menu (bottom-left stack, completely clear of Uncheck Portals)
        if (auto guidelinesMenu = this->getChildByID("guidelines-menu")) {
            guidelinesMenu->addChild(btn);
            guidelinesMenu->updateLayout();
        } else if (auto infoMenu = this->getChildByID("info-menu")) {
            infoMenu->addChild(btn);
            infoMenu->updateLayout();
        } else {
            auto winSize = CCDirector::sharedDirector()->getWinSize();
            auto fallbackMenu = CCMenu::create();
            fallbackMenu->setPosition({ 55.f, 95.f });
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