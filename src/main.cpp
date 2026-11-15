#include <Geode/Geode.hpp>
#include <Geode/modify/EditorPauseLayer.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>

using namespace geode::prelude;

class StripObjectsPopup : public geode::Popup<> {
protected:
    EditorUI* m_editorUI = nullptr;
    LevelEditorLayer* m_editorLayer = nullptr;
    geode::TextInput* m_countInput = nullptr;
    CCLabelBMFont* m_currentCountLabel = nullptr;

    bool setup(EditorUI* editorUI, LevelEditorLayer* editorLayer) {
        m_editorUI = editorUI;
        m_editorLayer = editorLayer;

        this->setTitle("Object Stripper");

        int currentCount = m_editorLayer->m_objects ? m_editorLayer->m_objects->count() : 0;

        m_currentCountLabel = CCLabelBMFont::create(
            fmt::format("Current Objects: {}", currentCount).c_str(),
            "goldFont.fnt"
        );
        m_currentCountLabel->setScale(0.55f);
        m_currentCountLabel->setPosition({ m_size.width / 2.f, m_size.height - 52.f });
        m_mainLayer->addChild(m_currentCountLabel);

        auto hintLabel = CCLabelBMFont::create("Target Object Limit:", "bigFont.fnt");
        hintLabel->setScale(0.42f);
        hintLabel->setPosition({ m_size.width / 2.f, m_size.height - 80.f });
        m_mainLayer->addChild(hintLabel);

        m_countInput = geode::TextInput::create(140.f, "e.g. 50", "bigFont.fnt");
        m_countInput->setCommonFilter(CommonFilter::Uint);
        m_countInput->setMaxCharCount(7);
        m_countInput->setPosition({ m_size.width / 2.f, m_size.height - 108.f });
        m_mainLayer->addChild(m_countInput);

        auto stripBtnSpr = ButtonSprite::create("Strip Excess", "goldFont.fnt", "GJ_button_06.png", 0.8f);
        auto stripBtn = CCMenuItemSpriteExtra::create(
            stripBtnSpr,
            this,
            menu_selector(StripObjectsPopup::onStrip)
        );
        stripBtn->setPosition({ m_size.width / 2.f, 34.f });
        m_buttonMenu->addChild(stripBtn);

        return true;
    }

    void onStrip(CCObject*) {
        if (!m_editorLayer || !m_editorUI || !m_editorLayer->m_objects) return;

        auto text = m_countInput->getString();
        if (text.empty()) {
            FLAlertLayer::create("Error", "Please enter a target object number.", "OK")->show();
            return;
        }

        auto parsed = geode::utils::numFromString<int>(text);
        if (!parsed.isOk() || parsed.unwrap() < 0) {
            FLAlertLayer::create("Error", "Invalid number entered.", "OK")->show();
            return;
        }

        int targetCount = parsed.unwrap();
        int totalObjects = m_editorLayer->m_objects->count();

        if (totalObjects <= targetCount) {
            FLAlertLayer::create(
                "Nothing to Strip",
                fmt::format("Level already has {} objects (limit: {}).", totalObjects, targetCount).c_str(),
                "OK"
            )->show();
            return;
        }

        std::vector<GameObject*> toDelete;
        int kept = 0;

        for (auto* obj : CCArrayExt<GameObject*>(m_editorLayer->m_objects)) {
            if (!obj) continue;

            bool isLocked = m_editorLayer->isLayerLocked(obj->m_editorLayer) ||
                            (obj->m_editorLayer2 != 0 && m_editorLayer->isLayerLocked(obj->m_editorLayer2));

            if (isLocked) {
                kept++;
                continue;
            }

            if (kept < targetCount) {
                kept++;
            } else {
                toDelete.push_back(obj);
            }
        }

        if (toDelete.empty()) {
            FLAlertLayer::create(
                "Protected Objects",
                "All remaining objects are on locked layers!",
                "OK"
            )->show();
            return;
        }

        m_editorUI->deselectAll();
        for (auto* obj : toDelete) {
            m_editorLayer->removeObject(obj, false);
        }

        m_editorUI->updateButtons();
        m_editorUI->updateObjectInfoLabel();

        int deletedCount = static_cast<int>(toDelete.size());
        int finalCount = m_editorLayer->m_objects->count();

        Notification::create(
            fmt::format("Deleted {} objects! ({} remaining)", deletedCount, finalCount),
            NotificationIcon::Success
        )->show();

        this->onClose(nullptr);
    }

public:
    static StripObjectsPopup* create(EditorUI* editorUI, LevelEditorLayer* editorLayer) {
        auto ret = new StripObjectsPopup();
        if (ret && ret->initAnchored(260.f, 185.f)) {
            ret->m_editorUI = editorUI;
            ret->m_editorLayer = editorLayer;
            ret->m_mainLayer->removeAllChildren();
            ret->m_buttonMenu->removeAllChildren();
            ret->setup(editorUI, editorLayer);
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
};

class $modify(StripEditorPauseLayer, EditorPauseLayer) {
    bool init(LevelEditorLayer* editorLayer) {
        if (!EditorPauseLayer::init(editorLayer)) return false;

        auto spr = ButtonSprite::create("Strip Obj", "goldFont.fnt", "GJ_button_06.png", 0.7f);
        spr->setScale(0.75f);

        auto btn = CCMenuItemSpriteExtra::create(
            spr,
            this,
            menu_selector(StripEditorPauseLayer::onOpenStripPopup)
        );
        btn->setID("strip-objects-btn"_spr);

        if (auto menu = this->getChildByID("guidelines-menu")) {
            menu->addChild(btn);
            menu->updateLayout();
        } else if (auto actionsMenu = this->getChildByID("actions-menu")) {
            actionsMenu->addChild(btn);
            actionsMenu->updateLayout();
        } else {
            auto fallbackMenu = CCMenu::create();
            fallbackMenu->setPosition({ 65.f, 45.f });
            fallbackMenu->addChild(btn);
            this->addChild(fallbackMenu);
        }

        return true;
    }

    void onOpenStripPopup(CCObject*) {
        if (m_editorLayer && m_editorLayer->m_editorUI) {
            StripObjectsPopup::create(m_editorLayer->m_editorUI, m_editorLayer)->show();
        }
    }
};