#include <cmath>
#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"
#include "sapphire_engine.hpp"

namespace Sapphire
{
    std::vector<SapphireModule*> SapphireModule::All;
    ModelInfo *ModelInfo::front;


    float ValidateNumber(
        float value,
        const char *sourceFileName,
        int sourceLineNumber,
        const char *functionName,
        const char *expression)
    {
        static constexpr unsigned logLimit = 100;
        static unsigned logCount = 0;

        if (!std::isfinite(value))
        {
            if (logCount < logLimit)
            {
                ++logCount;
                WARN("(%u/%u) Nonfinite value %g encountered at: %s [%d] %s --> '%s'",
                    logCount,
                    logLimit,
                    value,
                    sourceFileName,
                    sourceLineNumber,
                    functionName,
                    expression
                );
            }
        }
        return value;
    }


    struct NeonBorderState
    {
        int64_t sapphireModuleId{};
        bool neonMode{};

        explicit NeonBorderState(int64_t moduleId, bool neon)
            : sapphireModuleId(moduleId)
            , neonMode(neon)
            {}
    };

    struct ToggleAllNeonBordersAction : history::Action
    {
        std::vector<NeonBorderState> stateList;     // what each neon mode used to be, per module
        bool neon{};    // what we changed all the modules' neon modes to

        explicit ToggleAllNeonBordersAction()
        {
            name = "toggle neon borders in all Sapphire modules";
        }

        void append(const SapphireModule* smod)
        {
            if (smod)
                stateList.push_back(NeonBorderState(smod->id, smod->neonMode));
        }

        void undo() override
        {
            for (const NeonBorderState& state : stateList)
                if (SapphireModule* smod = FindSapphireModule(state.sapphireModuleId))
                    smod->neonMode = state.neonMode;
        }

        void redo() override
        {
            for (const NeonBorderState& state : stateList)
                if (SapphireModule* smod = FindSapphireModule(state.sapphireModuleId))
                    smod->neonMode = neon;
        }
    };

    void SapphireWidget::ToggleAllNeonBorders()
    {
        auto action = new ToggleAllNeonBordersAction;

        // Vote: how many modules have neon mode enabled, and how many disabled?
        int brightCount = 0;
        int darkCount = 0;
        for (const SapphireModule* smod : SapphireModule::All)
        {
            action->append(smod);
            smod->neonMode ? ++brightCount : ++darkCount;
        }

        // If more than half are enabled, turn all off. Otherwise turn all on.
        action->neon = (2*brightCount <= darkCount);
        action->redo();
        APP->history->push(action);
    }


    struct ToggleNeonBorderAction : history::Action
    {
        int64_t moduleId{};

        explicit ToggleNeonBorderAction(int64_t id)
            : moduleId(id)
        {
            name = "toggle neon border";
        }

        void toggle()
        {
            if (SapphireModule* smod = FindSapphireModule(moduleId))
                smod->neonMode = !smod->neonMode;
        }

        void undo() override { toggle(); }
        void redo() override { toggle(); }
    };


    void SapphireWidget::ToggleNeonBorder(SapphireModule *smod)
    {
        if (smod)
        {
            auto action = new ToggleNeonBorderAction(smod->id);
            action->redo();
            APP->history->push(action);
        }
    }

    void SapphireWidget::drawSplash(NVGcontext* vg, float x1)
    {
        if (!splash.active)
            return;

        double remaining = splash.remainingTime();
        if (remaining > 0)
        {
            double frac = splash.emphasis*(remaining / splash.durationSeconds);
            int opacity = std::clamp<int>(
                static_cast<int>(std::round(255*frac)),
                0,
                255
            );

            if (opacity > 0)
            {
                // Draw a box over the whole panel with gradually decreasing opacity.
                NVGcolor color = nvgRGBA(splash.rgb[0], splash.rgb[1], splash.rgb[2], opacity);
                nvgBeginPath(vg);
                nvgRect(vg, mm2px(x1), 0, box.size.x - mm2px(x1), box.size.y);
                nvgFillColor(vg, color);
                nvgFill(vg);
            }
        }
        else
        {
            splash.end();
        }
    }

    static void DrawBorder(NVGcontext* vg, NVGcolor color, float x1, float y1, float dx, float dy)
    {
        nvgBeginPath(vg);
        nvgRect(vg, x1, y1, dx, dy);
        nvgFillColor(vg, color);
        nvgFill(vg);
    }


    static void DrawOpaqueBorders(NVGcontext* vg, const Rect& box, bool neon, bool hideLeft, bool hideRight)
    {
        const float margin = 1;
        const float vertical = box.size.y - 2*margin;
        const NVGcolor panelColor  = nvgRGB(0x4f, 0x8d, 0xf2);
        const NVGcolor borderColor = nvgRGB(0x5d, 0x43, 0xa3);

        if (hideLeft)
            DrawBorder(vg, panelColor, 0, margin, margin, vertical);

        if (hideRight)
            DrawBorder(vg, panelColor, box.size.x - margin, margin, margin + DX_REMOVE_GAP, vertical);

        if (!neon)
        {
            // Top border
            DrawBorder(vg, borderColor, 0, 0, box.size.x, margin);

            // Bottom border
            DrawBorder(vg, borderColor, 0, box.size.y - margin, box.size.x, margin);

            // Left border
            if (!hideLeft)
                DrawBorder(vg, borderColor, 0, margin, margin, vertical);

            // Right border
            if (!hideRight)
                DrawBorder(vg, borderColor, box.size.x - margin, margin, margin, vertical);
        }
    }


    NVGcolor SapphireWidget::getNeonColor()
    {
        // Adjust the neon color to fit the room brightness.
        NVGcolor c0 = nvgRGB(0xd4, 0x8f, 0xff);
        NVGcolor c1 = nvgRGB(0xfa, 0xc8, 0x40);
        const float m = Cube(rack::settings::rackBrightness);
        float r = (1-m)*c0.r + m*c1.r;
        float g = (1-m)*c0.g + m*c1.g;
        float b = (1-m)*c0.b + m*c1.b;
        return nvgRGBf(r, g, b);
    }


    static void DrawGlowingBorders(NVGcontext* vg, const Rect& box, bool hideLeft, bool hideRight)
    {
        const float margin = 1;
        const float vertical = box.size.y - 2*margin;
        const NVGcolor glowColor = SapphireWidget::getNeonColor();

        // Top border
        DrawBorder(vg, glowColor, 0, 0, box.size.x, margin);

        // Bottom border
        DrawBorder(vg, glowColor, 0, box.size.y - margin, box.size.x, margin);

        // Left border
        if (!hideLeft)
            DrawBorder(vg, glowColor, 0, margin, margin, vertical);

        // Right border
        if (!hideRight)
            DrawBorder(vg, glowColor, box.size.x - margin, margin, margin, vertical);
    }


    void SapphireWidget::draw(const DrawArgs& args)
    {
        ModuleWidget::draw(args);
        DrawOpaqueBorders(args.vg, box, isNeonModeActive(), isLeftBorderHidden(), isRightBorderHidden());
    }


    void SapphireWidget::drawLayer(const DrawArgs& args, int layer)
    {
        ModuleWidget::drawLayer(args, layer);
        if (layer == 1)
        {
            drawSplash(args.vg, splash.x1);
            if (isNeonModeActive())
                DrawGlowingBorders(args.vg, box, isLeftBorderHidden(), isRightBorderHidden());
        }
    }


    ModuleWidget* FindWidgetForId(int64_t moduleId)
    {
        for (Widget* w : APP->scene->rack->getModuleContainer()->children)
            if (auto mw = dynamic_cast<ModuleWidget*>(w))
                if (mw->module && mw->module->id == moduleId)
                    return mw;
        return nullptr;
    }


    Module* FindModuleForId(int64_t moduleId)
    {
        if (ModuleWidget *wid = FindWidgetForId(moduleId))
            return wid->module;
        return nullptr;
    }


    std::vector<PanelState> SnapshotPanelPositions()
    {
        std::vector<PanelState> list;
        for (Widget* w : APP->scene->rack->getModuleContainer()->children)
        {
            auto mw = dynamic_cast<ModuleWidget*>(w);
            if (mw && mw->module)
                list.push_back(PanelState(mw));
        }
        return list;
    }


    ModuleWidget* FindWidgetClosestOnRight(const ModuleWidget* origin, int hpDistanceLimit)
    {
        ModuleWidget *closest = nullptr;
        if (origin && hpDistanceLimit > 0)
        {
            const float ox = px2mm(origin->box.pos.x + origin->box.size.x);
            const float oy = px2mm(origin->box.pos.y);
            for (Widget* w : APP->scene->rack->getModuleContainer()->children)
            {
                auto mw = dynamic_cast<ModuleWidget*>(w);
                if (mw && mw->module)
                {
                    const float mx = px2mm(mw->box.pos.x);
                    const float my = px2mm(mw->box.pos.y);
                    const int row = railDistance(my - oy);
                    const int col = hpDistance(mx - ox);

                    // Find the leftmost module that is to the right of `origin`.
                    // Must be within hpDistanceLimit HP units to the right.
                    if (row == 0 && col > 0 && col <= hpDistanceLimit)
                        if (closest == nullptr || mx < px2mm(closest->box.pos.x))
                            closest = mw;
                }
            }
        }
        return closest;
    }


    static std::vector<PanelState> FindMovedPanels(const std::vector<PanelState>& allPanelPositions)
    {
        std::vector<PanelState> moved;
        for (const PanelState& p : allPanelPositions)
        {
            // Search for the matching module ID in the rack...
            ModuleWidget* widget = APP->scene->rack->getModule(p.moduleId);
            if (widget)
            {
                Vec newPos = widget->getPosition();
                if (newPos != p.oldPos)
                {
                    PanelState updated = p;
                    updated.newPos = newPos;
                    moved.push_back(updated);
                }
            }
        }

        // Sort by y, then x, in ascending order.
        // In practice, all the y values should be the same, so sort by x.
        std::sort(moved.begin(), moved.end());

        return moved;
    }


    SapphireModule* AddExpander(Model* model, ModuleWidget* parentModWidget, ExpanderDirection dir, bool clone)
    {
        std::vector<PanelState> allPanelPositions = SnapshotPanelPositions();

        Module* rawModule = model->createModule();
        assert(rawModule);
        SapphireModule* expanderModule = dynamic_cast<SapphireModule*>(rawModule);
        assert(expanderModule);
        if (clone)
        {
            if (parentModWidget->module)
            {
                // The caller is asking us to copy settings from the parent module to the new module.
                // We can do this generically if they are the same kind of module (if both have the same model)
                // by serializing/deserializing JSON.
                if (model == parentModWidget->model)
                {
                    json_t* js = parentModWidget->module->toJson();
                    expanderModule->fromJson(js);
                    json_decref(js);
                }
                else
                {
                    // Fallback for copying settings from different kinds of modules.
                    // Example: Echo can create an EchoTap, and the tape loop settings are the same.
                    // The virtual method tryCopySettingsFrom exists as a hack just for this case.
                    if (auto parentModule = dynamic_cast<SapphireModule*>(parentModWidget->module))
                        expanderModule->tryCopySettingsFrom(parentModule);
                }
            }
        }
        expanderModule->postInsertFilterHook();        // give a chance for cleanup, re-init, etc, after creating
        APP->engine->addModule(expanderModule);
        ModuleWidget* rawWidget = model->createModuleWidget(expanderModule);
        assert(rawWidget);
        SapphireWidget* sapphireWidget = dynamic_cast<SapphireWidget*>(rawWidget);
        assert(sapphireWidget);

        // When NOT cloning, honor the user's default settings!
        // Without this code, we end up with factory defaults no matter what the user does.
        if (!clone)         
            sapphireWidget->loadTemplate();

        float dx;
        if (dir == ExpanderDirection::Left)
        {
            // Inserting on the left is tricky, because the Rack SDK
            // wants to shift things to the right to make room.
            // If there is a gap big enough to fit the panel on the left,
            // target that gap, so nothing has to move.
            // Otherwise, leave dx==0 to cause the parent module to shift to the right.
            // That's not as pleasant, but required to keep the modules connected.
            float gx2 = parentModWidget->box.pos.x;
            float gx1 = gx2 - sapphireWidget->box.size.x;
            bool gap = true;
            for (const PanelState& p : allPanelPositions)
            {
                if (p.oldPos.y == parentModWidget->box.pos.y)
                {
                    float px1 = p.oldPos.x;
                    float px2 = p.oldPos.x + p.size.x;
                    if (gx1 >= px1 && gx1 < px2)
                        gap = false;
                    if (gx2 > px1 && gx2 <= px2)
                        gap = false;
                    if (px1 >= gx1 && px1 < gx2)
                        gap = false;
                    if (px2 > gx1 && px2 <= gx2)
                        gap = false;
                    if (!gap)
                        break;
                }
            }
            dx = gap ? -sapphireWidget->box.size.x : 0;
        }
        else
        {
            // When inserting to the right, just stick the new module
            // immediately to the right of this one.
            dx = parentModWidget->box.size.x;
        }

        APP->scene->rack->setModulePosForce(sapphireWidget, Vec{parentModWidget->box.pos.x + dx, parentModWidget->box.pos.y});
        APP->scene->rack->addModule(sapphireWidget);

        // Push this module creation action onto undo/redo stack.
        std::vector<PanelState> movedPanels = FindMovedPanels(allPanelPositions);
        APP->history->push(new AddExpanderAction(model, sapphireWidget, movedPanels));

        // Animate the first few frames of the new panel, like a splash screen.
        sapphireWidget->splash.begin(0xa5, 0x1f, 0xde);

        return expanderModule;
    }


    void SapphireWidget::createTooltip(SapphireTooltip*& tooltip, const std::string& text)
    {
        if (!settings::tooltips)
            return;

        if (tooltip)
            return;

        if (!module)
            return;

        tooltip = new SapphireTooltip;
        tooltip->text = text;
        APP->scene->addChild(tooltip);
    }

    void SapphireWidget::destroyTooltip(SapphireTooltip*& tooltip)
    {
        if (tooltip)
        {
            APP->scene->removeChild(tooltip);
            delete tooltip;
            tooltip = nullptr;
        }
    }

    void SapphireWidget::updateTooltip(
        bool& flag, bool state, SapphireTooltip*& tooltip, const std::string& text)
    {
        if (state != flag)
        {
            if (state)
                createTooltip(tooltip, text);
            else
                destroyTooltip(tooltip);
            flag = state;
        }
    }

    void SapphireWidget::addEnvelopeFollowerLabels()
    {
        envLabel = addEnvelopeLabel("env", true);
        envSelLabel = addEnvelopeLabel("env_sel");
        dckLabel = addEnvelopeLabel("dck");
        dckSelLabel = addEnvelopeLabel("dck_sel");
    }

    void SapphireWidget::updateEnvDuck()
    {
        const SapphireModule* smod = getSapphireModule();
        const bool duck = smod && smod->duck();

        if (envLabel)
            envLabel->setVisible(!duck && !hilightEnvDuckButton);

        if (envSelLabel)
            envSelLabel->setVisible(!duck && hilightEnvDuckButton);

        if (dckLabel)
            dckLabel->setVisible(duck && !hilightEnvDuckButton);

        if (dckSelLabel)
            dckSelLabel->setVisible(duck && hilightEnvDuckButton);
    }

    bool SapphireWidget::isInsideEnvDuckButton(Vec pos) const
    {
        if (envDuckLabelPos.y <= 0)
            return false;       // no ENV/DCK resource generated for this module?

        const float dx = pos.x - envDuckLabelPos.x;
        const float dy = pos.y - envDuckLabelPos.y;
        const float rectWidth = mm2px(8.0);
        const float rectHeight = mm2px(4.5);
        return (std::abs(dx) <= rectWidth/2) && (std::abs(dy) <= rectHeight/2);
    }

    void SapphireWidget::appendContextMenu(Menu* menu)
    {
        if (SapphireModule* sm = getSapphireModule())
        {
            menu->addChild(new MenuSeparator);

            if (sm->includeNeonModeMenuItem)
            {
                menu->addChild(createMenuItem(
                    "Toggle neon borders (this module only)",
                    "",
                    [=]() { ToggleNeonBorder(sm); }
                ));
            }

            menu->addChild(createMenuItem(
                "Toggle neon borders in all Sapphire modules",
                "",
                ToggleAllNeonBorders
            ));

            if (sm->dcRejectQuantity)
                menu->addChild(new DcRejectSlider(sm->dcRejectQuantity));

            if (sm->enableLimiterMenuItems)
                sm->addLimiterMenuItems(menu);

            if (shouldOfferTricorder())
            {
                menu->addChild(createMenuItem(
                    "Insert Tricorder on right",
                    "",
                    [this]{ addTricorderExpander(); }
                ));
            }

            if (shouldOfferChaops())
            {
                menu->addChild(createMenuItem(
                    "Insert Chaops on left",
                    "",
                    [this]{ addChaopsExpander(); }
                ));
            }

            if (IsShiftKeyPressed())
            {
                // Offer advanced / internal development tools to any Sapphire modules that opt in.
                if (sm->shouldOfferFireDrill && !sm->fireDrillTrigger)
                {
                    menu->addChild(createMenuItem(
                        "[TEST] Simulate NAN output (one shot)",
                        "",
                        [sm](){ sm->fireDrillTrigger = true; }
                    ));
                }
            }
        }
    }


    bool SapphireWidget::shouldOfferTricorder()
    {
        if (auto smod = getSapphireModule())
        {
            return
                ModelInfo::hasRole(smod, ExpanderRole::VectorSender) &&
                !IsModelType(smod->rightExpander.module, modelSapphireTricorder);
        }
        return false;
    }

    void SapphireWidget::addTricorderExpander()
    {
        AddExpander(modelSapphireTricorder, this, ExpanderDirection::Right, false);
    }

    bool SapphireWidget::shouldOfferChaops()
    {
        if (auto smod = getSapphireModule())
        {
            return
                ModelInfo::hasRole(smod, ExpanderRole::ChaosOpReceiver) &&
                !IsModelType(smod->leftExpander.module, modelSapphireChaops);
        }
        return false;
    }

    void SapphireWidget::addChaopsExpander()
    {
        AddExpander(modelSapphireChaops, this, ExpanderDirection::Left, false);
    }

    ToggleAllSensitivityAction::ToggleAllSensitivityAction(SapphireModule* sapphireModule)
    {
        name = "toggle sensitivity of all attenuverters";
        if (sapphireModule)
        {
            moduleId = sapphireModule->id;

            const int nparams = static_cast<int>(sapphireModule->paramInfo.size());
            for (int paramId = 0; paramId < nparams; ++paramId)
            {
                if (sapphireModule->isAttenuverter(paramId))
                {
                    prevStateList.push_back(SensitivityState(
                        paramId,
                        sapphireModule->isLowSensitive(paramId)
                    ));
                }
            }
        }
    }

    void ToggleAllSensitivityAction::redo()
    {
        if (SapphireModule* sapphireModule = FindSapphireModule(moduleId))
        {
            // Find all attenuverter knobs and toggle their low-sensitivity state together.
            const int nparams = static_cast<int>(sapphireModule->paramInfo.size());
            int countEnabled = 0;
            int countDisabled = 0;
            for (int paramId = 0; paramId < nparams; ++paramId)
                if (sapphireModule->isAttenuverter(paramId))
                    sapphireModule->isLowSensitive(paramId) ? ++countEnabled : ++countDisabled;

            // Let the knobs "vote". If a supermajority are enabled,
            // then we turn them all off.
            // Otherwise we turn them all on.
            const bool toggle = (countEnabled <= countDisabled);
            for (int paramId = 0; paramId < nparams; ++paramId)
                if (sapphireModule->isAttenuverter(paramId))
                    sapphireModule->setLowSensitive(paramId, toggle);
        }
    }

    void ToggleAllSensitivityAction::undo()
    {
        if (SapphireModule* sapphireModule = FindSapphireModule(moduleId))
            for (const SensitivityState& s : prevStateList)
                sapphireModule->setLowSensitive(s.paramId, s.lowSensitivity);
    }

    MenuItem* SapphireModule::createLimiterWarningLightMenuItem()
    {
        return createBoolMenuItem(
            "Limiter warning light",
            "",
            [=]()
            {
                return enableLimiterWarning;
            },
            [=](bool value)
            {
                if (value != enableLimiterWarning)
                    InvokeAction(new BoolToggleAction(enableLimiterWarning, "limiter warning light"));
            }
        );
    }

    void SapphireModule::addLimiterMenuItems(Menu* menu)
    {
        if (agcLevelQuantity)
        {
            menu->addChild(new SapphireSlider(agcLevelQuantity, "adjust output limiter voltage"));
            menu->addChild(createLimiterWarningLightMenuItem());
        }
    }


    void SapphireModule::addPolyphonicEnvelopeMenuItem(Menu* menu)
    {
        if (envelopeFollower.enabled)
        {
            menu->addChild(new MenuSeparator);
            menu->addChild(createBoolMenuItem(
                "Polyphonic envelope output",
                "",
                [=]{ return envelopeFollower.polyphonicOutput; },
                [=](bool state){ setPolyphonicEnvelopeOutput(state); }
            ));
        }
    }

    void SapphireModule::setPolyphonicEnvelopeOutput(bool state)
    {
        if (envelopeFollower.enabled && envelopeFollower.polyphonicOutput != state)
            InvokeAction(new BoolToggleAction(envelopeFollower.polyphonicOutput, "mono/polyphonic envelope output"));
    }

    void SapphireModule::toggleEnvDuck()
    {
        if (envelopeFollower.enabled)
            InvokeAction(new BoolToggleAction(envelopeFollower.duck, "envelope/duck"));
    }

    void EnvelopeOutputPort::appendContextMenu(Menu* menu)
    {
        SapphirePort::appendContextMenu(menu);
        if (auto smod = dynamic_cast<SapphireModule*>(module))
        {
            smod->addPolyphonicEnvelopeMenuItem(menu);
        }
    }

    MenuItem* BoolToggleAction::CreateMenuItem(
        bool& flag,
        const std::string& menuItemText,
        const std::string& toggledThing)
    {
        return createBoolMenuItem(
            menuItemText,
            "",
            [&flag]()
            {
                return flag;
            },
            [&flag, toggledThing](bool value)
            {
                if (value != flag)
                    InvokeAction(new BoolToggleAction(flag, toggledThing));
            }
        );
    }


    void SapphireModule::onAdd(const AddEvent& e)
    {
        Module::onAdd(e);

        if (std::find(All.begin(), All.end(), this) == All.end())
            All.push_back(this);
    }



    void SapphireModule::onRemove(const RemoveEvent& e)
    {
        // We keep a list of all active Sapphire modules.
        // This is needed for "toggle neon mode in all Sapphire modules".
        // Remove this module from the list.
        All.erase(std::remove(All.begin(), All.end(), this), All.end());

        // Give any other Sapphire modules, widgets, controls, etc.,
        // a chance to find out that the module they are pointing
        // to is about to be destroyed. This is their chance to sever
        // links to soon-to-be-invalid memory.

        for (RemovalSubscriber* s : removalSubscriberList)
            s->disconnect();

        removalSubscriberList.clear();

        Module::onRemove(e);
    }


    void SapphireModule::subscribe(RemovalSubscriber* subscriber)
    {
        if (subscriber)
        {
            // *** MEMORY SAFETY ***
            // The subscriber wants its address to be remembered by this module.
            // Later, when this SapphireModule is being removed, onRemove() will
            // call subscriber->disconnect() to sever any connection with this
            // module's memory, and vice versa.

            auto existing = std::find(
                removalSubscriberList.begin(),
                removalSubscriberList.end(),
                subscriber
            );

            if (existing == removalSubscriberList.end())
                removalSubscriberList.push_back(subscriber);
        }
    }


    void SapphireModule::unsubscribe(RemovalSubscriber* subscriber)
    {
        if (subscriber)
        {
            // *** MEMORY SAFETY ***
            // Unsubscribing means `subscriber` is telling us it is about to be destroyed.
            // Go ahead and disconnect the subscriber now,
            // since we won't be able to do it later.
            subscriber->disconnect();

            // Delete this subscriber pointer from the list,
            // because the memory it points to is about to be freed.
            removalSubscriberList.erase(
                std::remove(removalSubscriberList.begin(), removalSubscriberList.end(), subscriber),
                removalSubscriberList.end()
            );
        }
    }


    MenuItem* SapphireModule::createStereoSplitterMenuItem()
    {
        return BoolToggleAction::CreateMenuItem(
            enableStereoSplitter,
            "Enable input stereo splitter",
            "input stereo splitter"
        );
    }

    MenuItem* SapphireModule::createStereoMergeMenuItem()
    {
        return BoolToggleAction::CreateMenuItem(
            enableStereoMerge,
            "Send polyphonic stereo to L output",
            "polyphonic output"
        );
    }


    void SliderAction::setParameterValue(float value)
    {
        if (SapphireModule* module = FindSapphireModule(moduleId))
            if (ParamQuantity* qty = module->getParamQuantity(paramId))
                qty->setValue(value);
    }

    // Create ModulePresetPathItems for each patch in a directory.
    void AppendFactoryPresets(ui::Menu *menu, WeakPtr<ModuleWidget> moduleWidget, std::string presetDir)
    {
        if (system::isDirectory(presetDir))
        {
            // Note: This is not cached, so opening this menu each time might have a bit of latency.
            std::vector<std::string> entries = system::getEntries(presetDir);
            std::sort(entries.begin(), entries.end());
            for (std::string path : entries)
            {
                std::string name = system::getStem(path);
                if (system::isDirectory(path))
                {
                    menu->addChild(createSubmenuItem(
                        name,
                        "",
                        [=](ui::Menu *menu)
                        {
                            if (moduleWidget)
                                AppendFactoryPresets(menu, moduleWidget, path);
                        }
                    ));
                }
                else if (system::getExtension(path) == ".vcvm" && name != "template")
                {
                    menu->addChild(createMenuItem(
                        name,
                        "",
                        [=]()
                        {
                            if (moduleWidget)
                            {
                                try
                                {
                                    moduleWidget->loadAction(path);
                                }
                                catch (Exception& e)
                                {
                                    WARN("Cannot load preset [%s]: %s", path.c_str(), e.what());
                                }
                            }
                        }
                    ));
                }
            }
        }
    };


    const Model* PeekAdjacentModel(const ModuleWidget* origin, ExpanderDirection dir)
    {
        if (origin)
        {
            float oy = px2mm(origin->box.pos.y);
            float ox = px2mm(origin->box.pos.x);
            if (dir == ExpanderDirection::Right)
                ox += px2mm(origin->box.size.x);

            for (const Widget* w : APP->scene->rack->getModuleContainer()->children)
            {
                if (auto mw = dynamic_cast<const ModuleWidget*>(w))
                {
                    float my = px2mm(mw->box.pos.y);
                    float mx = px2mm(mw->box.pos.x);
                    if (dir == ExpanderDirection::Left)
                        mx += px2mm(mw->box.size.x);
                    int row = railDistance(my - oy);
                    int col = hpDistance(mx - ox);
                    if (row==0 && col==0)
                        return mw->model;
                }
            }
        }
        return nullptr;
    }


    void VectorInsertButton::onButton(const event::Button& e)
    {
        app::SvgSwitch::onButton(e);
        if (parentWidget && expanderModel)
        {
            if (e.action == GLFW_RELEASE && e.button == GLFW_MOUSE_BUTTON_LEFT)
            {
                // Look in the requested insertion direction to see if
                // the desired model is already present.
                // If so, suppress adding a redundant instance of that model.
                const Model* peekModel = PeekAdjacentModel(parentWidget, direction);
                if (expanderModel != peekModel)
                    AddExpander(expanderModel, parentWidget, direction, false);
            }
        }
    }

    void SapphireAttenuverterKnob::appendContextMenu(Menu* menu)
    {
        Trimpot::appendContextMenu(menu);
        if (context)
        {
            menu->addChild(createBoolMenuItem(
                "Low sensitivity",
                "",
                [=]() -> bool       // getter
                {
                    return context->lowSensitivityMode;
                },
                [=](bool state)     // setter
                {
                    if (state != context->lowSensitivityMode)
                        InvokeAction(new BoolToggleAction(context->lowSensitivityMode, "attenuverter sensitivity"));
                }
            ));

            menu->addChild(createBoolMenuItem(
                "Unipolar mode",
                "",
                [=]() -> bool       // getter
                {
                    return context->unipolar;
                },
                [=](bool state)     // setter
                {
                    if (state != context->unipolar)
                        InvokeAction(new BoolToggleAction(context->unipolar, "unipolar mode"));
                }
            ));
        }
    }

    ChaosModulationInfo SapphireAttenuverterKnob::getChaosModulationInfo()
    {
        ChaosModulationInfo info;
        if (SapphireModule* smod = dynamic_cast<SapphireModule*>(module))
        {
            if (context && context->supportsChaos && smod->shouldDisplayChaosVoltages())
            {
                info.voltage[0] = context->chaosVoltage[0];
                info.voltage[1] = context->chaosVoltage[1];

                if (context->inputPortId < smod->inputs.size())
                    if (!smod->inputs.at(context->inputPortId).isConnected())     // chaos works only when the CV input port has no cables
                        if (ParamQuantity* qty = getParamQuantity())
                            info.isActive = (qty->getValue() != 0);
            }
        }
        return info;
    }


    void SapphireAttenuverterKnob::drawLayer(const DrawArgs& args, int layer)
    {
        Trimpot::drawLayer(args, layer);

        if (layer == 1)
        {
            if (isLowSensitive())
            {
                // Draw a small dot on top of the knob to indicate that it is in low-sensitivity mode.
                nvgBeginPath(args.vg);
                nvgStrokeColor(args.vg, SCHEME_RED);
                nvgFillColor(args.vg, SCHEME_ORANGE);
                nvgCircle(args.vg, box.size.x/2, box.size.y/2, 3.5);
                nvgStroke(args.vg);
                nvgFill(args.vg);
            }

            if (isUnipolarMode())
            {
                constexpr float radiusPx = 2.25;
                constexpr float stemHeightPx = 3.0;

                const float xc = xSatellite();
                const float yc = ySatellite();

                // Draw something that looks like a little letter 'u' near the knob.
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, xc-radiusPx, yc-stemHeightPx);
                nvgLineTo(args.vg, xc-radiusPx, yc);
                nvgArc(args.vg, xc, yc, radiusPx, NVG_PI, 0, NVG_CCW);
                nvgLineTo(args.vg, xc+radiusPx, yc-stemHeightPx);
                nvgStrokeWidth(args.vg, 1.25);
                nvgStrokeColor(args.vg, SCHEME_YELLOW);
                nvgLineCap(args.vg, NVG_ROUND);
                nvgStroke(args.vg);
            }

            const ChaosModulationInfo chaos = getChaosModulationInfo();
            if (chaos.isActive)
            {
                constexpr float radiusPx = 9.5;

                nvgStrokeWidth(args.vg, 1.5);
                nvgLineCap(args.vg, NVG_BUTT);

                if (chaos.voltage[0] == chaos.voltage[1])
                {
                    // Draw a luminous ring around the knob using a single red/green color.
                    nvgBeginPath(args.vg);
                    nvgStrokeColor(args.vg, VoltageColor(chaos.voltage[0]));
                    nvgCircle(args.vg, box.size.x/2, box.size.y/2, radiusPx);
                    nvgStroke(args.vg);
                }
                else
                {
                    // Split the luminous ring into a left semicircle and a right semicircle.
                    // Now each half of the ring has its own color.
                    // We use visible left/right to represent stereo left/right.

                    // Left half-ring for the stereo left channel.
                    nvgBeginPath(args.vg);
                    nvgStrokeColor(args.vg, VoltageColor(chaos.voltage[0]));
                    nvgArc(args.vg, box.size.x/2, box.size.y/2, radiusPx, M_PI/2, 3*M_PI/2, NVG_CW);
                    nvgStroke(args.vg);

                    // Right half-ring for the stereo right channel.
                    nvgBeginPath(args.vg);
                    nvgStrokeColor(args.vg, VoltageColor(chaos.voltage[1]));
                    nvgArc(args.vg, box.size.x/2, box.size.y/2, radiusPx, M_PI/2, 3*M_PI/2, NVG_CCW);
                    nvgStroke(args.vg);
                }
            }
        }
    }

    void SnapVoctAttenuverterKnob::drawLayer(const DrawArgs& args, int layer)
    {
        SapphireAttenuverterKnob::drawLayer(args, layer);

        if (layer == 1)
        {
            if (isVoct())
            {
                // Draw a glowing 'V' at the satellite offset.
                // V/OCT requires unipolar mode be disabled, so there is no possible conflict with 'U'.
                const float xc = xSatellite();
                const float yc = ySatellite();
                const float dx = 2.0;
                const float dy = 2.5;
                const float x1 = xc - dx;
                const float x2 = xc;
                const float x3 = xc + dx;
                const float yTop = yc - dy;
                const float yBottom = yc + dy;

                nvgBeginPath(args.vg);
                nvgStrokeWidth(args.vg, 1.25);
                nvgStrokeColor(args.vg, SCHEME_CYAN);
                nvgLineCap(args.vg, NVG_ROUND);
                nvgMoveTo(args.vg, x1, yTop);
                nvgLineTo(args.vg, x2, yBottom);
                nvgLineTo(args.vg, x3, yTop);
                nvgStroke(args.vg);
            }
        }
    }

    void InitChainAction::undo()
    {
        for (const InitChainNode& node : list)
            if (Module* module = APP->engine->getModule(node.moduleId))
                APP->engine->moduleFromJson(module, node.json);
    }

    void InitChainAction::redo()
    {
        for (const InitChainNode& node : list)
            if (Module* module = APP->engine->getModule(node.moduleId))
                APP->engine->resetModule(module);
    }

    NVGcolor VoltageColor(float voltage)
    {
        if (!std::isfinite(voltage))
            return SCHEME_PURPLE;

        float u = std::clamp<float>(std::abs(voltage)/10, 0, 1);
        float k = 1 - Cube(1-u);
        float r = (voltage > 0) ? 0 : k;
        float g = (voltage > 0) ? k : 0;
        return nvgRGBAf(r, g, 0, 0.9);
    }
}