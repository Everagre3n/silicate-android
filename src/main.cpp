// Silicate: Geode UI layer. All level logic lives in src/core (dependency-free, natively tested).
#include <Geode/Geode.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/LocalLevelManager.hpp>
#include <Geode/modify/LevelBrowserLayer.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/web.hpp>

#include <chrono>

#include "core/builder.hpp"
#include "core/planner.hpp"

using namespace geode::prelude;

class SilicatePopup : public geode::Popup {
protected:
    TextInput* m_prompt = nullptr;
    TextInput* m_key = nullptr;
    CCLabelBMFont* m_status = nullptr;
    CCMenuItemSpriteExtra* m_goBtn = nullptr;
    async::TaskHolder<web::WebResponse> m_task;
    bool m_busy = false;

    bool setup() {
        setTitle("Silicate AI Level Generator");
        auto size = m_mainLayer->getContentSize();

        m_prompt = TextInput::create(300.f, "Describe your level (e.g. icy hard level with a ship drop)");
        m_prompt->setMaxCharCount(200);
        m_prompt->setPosition({size.width / 2, size.height - 62.f});
        m_mainLayer->addChild(m_prompt);

        m_key = TextInput::create(300.f, "Gemini API key (saved in settings)");
        m_key->setPasswordMode(true);
        m_key->setPosition({size.width / 2, size.height - 102.f});
        m_key->setString(Mod::get()->getSettingValue<std::string>("api-key"));
        m_mainLayer->addChild(m_key);

        auto model = Mod::get()->getSettingValue<std::string>("model");
        auto diff = Mod::get()->getSettingValue<std::string>("difficulty");
        auto info = CCLabelBMFont::create(("Model: " + model + "   Difficulty: " + diff).c_str(), "chatFont.fnt");
        info->setScale(0.7f);
        info->setOpacity(160);
        info->setPosition({size.width / 2, size.height - 132.f});
        m_mainLayer->addChild(info);

        m_status = CCLabelBMFont::create("", "chatFont.fnt");
        m_status->setScale(0.7f);
        m_status->setPosition({size.width / 2, 58.f});
        m_mainLayer->addChild(m_status);

        m_goBtn = CCMenuItemSpriteExtra::create(ButtonSprite::create("Generate"), this, menu_selector(SilicatePopup::onGenerate));
        m_goBtn->setPosition({size.width / 2, 28.f});
        m_buttonMenu->addChild(m_goBtn);
        return true;
    }

    void setStatus(std::string const& s) { m_status->setString(s.c_str()); }

    void onGenerate(CCObject*) {
        if (m_busy) return;
        std::string key = m_key->getString();
        if (key.empty()) return setStatus("Enter your Gemini API key first.");
        Mod::get()->setSettingValue<std::string>("api-key", key);

        auto model = Mod::get()->getSettingValue<std::string>("model");
        auto diff = Mod::get()->getSettingValue<std::string>("difficulty");
        auto user = silicate::userPrompt(m_prompt->getString(), diff, "", 0);

        web::WebRequest req;
        req.header("x-goog-api-key", key);
        req.header("Content-Type", "application/json");
        req.bodyString(silicate::requestBody(user));
        req.timeout(std::chrono::seconds(90));

        m_busy = true;
        setStatus("Asking " + model + " for a plan...");
        m_task.spawn(req.post(silicate::endpointFor(model)), [this, diff](web::WebResponse res) {
            m_busy = false;
            if (!res.ok()) {
                return setStatus(res.code() > 0
                    ? silicate::describeError(res.code(), res.string().unwrapOr(""))
                    : "Network error - check your connection.");
            }
            try {
                uint64_t seed = (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count();
                silicate::Rng rng(seed);
                auto plan = silicate::parseResponse(res.string().unwrapOr(""), rng);
                plan.difficulty = diff;
                finish(plan, seed);
            } catch (std::exception const& ex) {
                setStatus(std::string("Failed: ") + ex.what());
            }
        });
    }

    void finish(silicate::Plan const& plan, uint64_t seed) {
        auto level = silicate::compileLevel(plan, seed);
        auto packed = ZipUtils::compressString(level.raw(), false, 11);

        auto gj = GJGameLevel::create();
        gj->m_levelName = plan.name;
        gj->m_levelDesc = "";
        gj->m_levelString = packed;
        gj->m_levelType = GJLevelType::Editor;
        gj->m_isEditable = true;
        gj->m_levelVersion = 1;
        LocalLevelManager::get()->m_localLevels->insertObject(gj, 0);

        FLAlertLayer::create(
            "Level created",
            fmt::format("<cy>{}</c> ({}, {}, {} bpm, {} objects) was added to <cg>My Levels</c>.",
                        plan.name, plan.theme, plan.difficulty, plan.bpm, level.objs.size()),
            "OK")->show();
        this->onClose(nullptr);
        auto scene = LevelBrowserLayer::scene(GJSearchObject::create(SearchType::MyLevels));
        CCDirector::get()->replaceScene(CCTransitionFade::create(0.5f, scene));
    }

public:
    static SilicatePopup* create() {
        auto ret = new SilicatePopup();
        if (ret->Popup::init(340.f, 190.f) && ret->setup()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

class $modify(SilicateBrowser, LevelBrowserLayer) {
    bool init(GJSearchObject* search) {
        if (!LevelBrowserLayer::init(search)) return false;
        if (!search || search->m_searchType != SearchType::MyLevels) return true;

        auto menu = CCMenu::create();
        menu->setID("silicate-menu"_spr);
        auto btn = CCMenuItemSpriteExtra::create(ButtonSprite::create("AI"), this, menu_selector(SilicateBrowser::onSilicate));
        menu->addChild(btn);
        auto win = CCDirector::get()->getWinSize();
        menu->setPosition({win.width - 40.f, 60.f});
        this->addChild(menu);
        return true;
    }

    void onSilicate(CCObject*) {
        if (auto p = SilicatePopup::create()) p->show();
    }
};
