#include <Geode/Geode.hpp>
#include <Geode/modify/EditorPauseLayer.hpp>
#include <vector>
#include <string>
#include <algorithm>

using namespace geode::prelude;

static bool isGameplayObject(GameObject* obj) {
    if (!obj) return false;

    // 1. Protect any non-decoration object type (solids, hazards, portals, orbs, pads, slopes, collectibles)
    if (obj->m_objectType != GameObjectType::Decoration) {
        return true;
    }

    // 2. Protect Start Pos (ID 31) and triggers
    if (obj->m_objectID == 31 || obj->m_isTrigger) {
        return true;
    }

    // 3. Protect any object assigned to a Group ID so Move/Alpha/Toggle/Animate triggers
    // (such as Deadlocked's bosses and moving platforms) never crash on Save & Play
    if (obj->m_groupCount > 0) {
        return true;
    }

    return false;
}

class StripObjectsPopup : public geode::Popup {
protected:
    EditorUI* m_editorUI = nullptr;
    TextInput* m_countInput = nullptr;
    CCLabelBMFont* m_currentCountLabel = nullptr;
    CCLabelBMFont* m_statusLabel = nullptr;
    CCMenuItemToggler* m_decoOnlyToggle = nullptr;
    CCMenuItemSpriteExtra* m_stripBtn = nullptr;

    bool m_decoOnly = false;
    bool m_isStripping = false;

    std::vector<Ref<GameObject>> m_queue;
    size_t m_cursor = 0;
    size_t m_requestedDelete = 0;

    static constexpr size_t BATCH_SIZE = 2500;

    bool init(EditorUI* editorUI) {
        if (!Popup::init(280.f, 225.f)) {
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
        m_decoOnlyToggle->setPosition({ m_size.width / 2.f - 62.f, m_size.height - 138.f });
        m_buttonMenu->addChild(m_decoOnlyToggle);

        auto decoLabel = CCLabelBMFont::create("Strip Deco Only", "bigFont.fnt");
        decoLabel->setScale(0.42f);
        decoLabel->setAnchorPoint({ 0.f, 0.5f });
        decoLabel->setPosition({ m_size.width / 2.f - 44.f, m_size.height - 138.f });
        m_mainLayer->addChild(decoLabel);

        m_statusLabel = CCLabelBMFont::create("", "chatFont.fnt");
        m_statusLabel->setScale(0.65f);
        m_statusLabel->setPosition({ m_size.width / 2.f, 62.f });
        m_statusLabel->setVisible(false);
        m_mainLayer->addChild(m_statusLabel);

        auto stripSpr = ButtonSprite::create("Strip", "goldFont.fnt", "GJ_button_06.png", 0.8f);
        m_stripBtn = CCMenuItemSpriteExtra::create(
            stripSpr,
            this,
            menu_selector(StripObjectsPopup::onStrip)
        );
        m_stripBtn->setPosition({ m_size.width / 2.f, 32.f });
        m_buttonMenu->addChild(m_stripBtn);

        return true;
    }

    void onToggleDecoOnly(CCObject*) {
        if (m_isStripping) return;
        m_decoOnly = !m_decoOnly;
    }

    void onStrip(CCObject*) {
        if (m_isStripping) return;

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

        m_requestedDelete = totalObjects - targetCount;
        m_queue.clear();
        m_queue.reserve(m_requestedDelete);
        m_cursor = 0;

        for (int i = static_cast<int>(totalObjects) - 1; i >= 0 && m_queue.size() < m_requestedDelete; --i) {
            auto obj = static_cast<GameObject*>(editorLayer->m_objects->objectAtIndex(i));
            if (!obj) continue;

            if (editorLayer->isLayerLocked(obj->m_editorLayer) ||
                (obj->m_editorLayer2 != 0 && editorLayer->isLayerLocked(obj->m_editorLayer2))) {
                continue;
            }

            if (m_decoOnly && isGameplayObject(obj)) {
                continue;
            }

            m_queue.emplace_back(obj);
        }

        if (m_queue.empty()) {
            std::string reason = m_decoOnly
                ? "All objects are gameplay, trigger-grouped, or on locked layers."
                : "All objects are on locked layers.";
            FLAlertLayer::create("Nothing to Strip", reason, "OK")->show();
            return;
        }

        // Lock controls and begin frame-chunked deletion
        m_isStripping = true;
        m_stripBtn->setEnabled(false);
        m_decoOnlyToggle->setEnabled(false);
        m_countInput->setEnabled(false);
        if (m_closeBtn) m_closeBtn->setEnabled(false);

        if (m_editorUI) {
            m_editorUI->deselectAll();
        }

        m_statusLabel->setVisible(true);
        m_statusLabel->setString(fmt::format("Stripping 0 / {}...", m_queue.size()).c_str());

        this->schedule(schedule_selector(StripObjectsPopup::processBatch));
    }

    void processBatch(float) {
        auto editorLayer = LevelEditorLayer::get();
        if (!editorLayer || !editorLayer->m_objects) {
            this->finishStripping();
            return;
        }

        size_t totalToRemove = m_queue.size();
        size_t endIdx = std::min(m_cursor + BATCH_SIZE, totalToRemove);

        for (; m_cursor < endIdx; ++m_cursor) {
            if (auto* obj = m_queue[m_cursor].data()) {
                editorLayer->removeObject(obj, false);
            }
        }

        size_t currentCount = editorLayer->m_objects->count();
        int percent = totalToRemove > 0
            ? static_cast<int>((m_cursor * 100) / totalToRemove)
            : 100;

        m_currentCountLabel->setString(fmt::format("Current Objects: {}", currentCount).c_str());
        m_statusLabel->setString(
            fmt::format("Stripping {} / {} ({}%)", m_cursor, totalToRemove, percent).c_str()
        );

        if (m_cursor >= totalToRemove) {
            this->finishStripping();
        }
    }

    void finishStripping() {
        this->unschedule(schedule_selector(StripObjectsPopup::processBatch));

        size_t removedCount = m_cursor;
        size_t requested = m_requestedDelete;
        bool decoOnly = m_decoOnly;

        m_queue.clear();
        m_queue.shrink_to_fit();
        m_isStripping = false;

        if (m_editorUI) {
            m_editorUI->updateButtons();
            m_editorUI->updateObjectInfoLabel();
        }

        auto editorLayer = LevelEditorLayer::get();
        size_t remaining = (editorLayer && editorLayer->m_objects)
            ? editorLayer->m_objects->count()
            : 0;

        if (removedCount < requested) {
            std::string reason = decoOnly
                ? "remaining objects are <cy>gameplay</c>, <cy>grouped</c>, or on <cr>locked layers</c>."
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

    void keyBackClicked() override {
        if (!m_isStripping) {
            Popup::keyBackClicked();
        }
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

        if (auto guidelinesMenu = this->getChildByID("guidelines-menu")) {
            guidelinesMenu->addChild(btn);
            guidelinesMenu->updateLayout();
        } else if (auto infoMenu = this->getChildByID("info-menu")) {
            infoMenu->addChild(btn);
            infoMenu->updateLayout();
        } else {
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