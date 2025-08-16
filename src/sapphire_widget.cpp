#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"

namespace Sapphire
{
    std::vector<SapphireModule*> SapphireModule::All;

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
            DrawBorder(vg, panelColor, box.size.x - margin, margin, margin + DxRemoveGap, vertical);

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

    const NVGcolor SapphireWidget::neonColor = nvgRGB(0xd4, 0x8f, 0xff);


    static void DrawGlowingBorders(NVGcontext* vg, const Rect& box, bool hideLeft, bool hideRight)
    {
        const float margin = 1;
        const float vertical = box.size.y - 2*margin;
        const NVGcolor glowColor = SapphireWidget::neonColor;

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
        DrawArgs newDrawArgs = args;

        // Eliminate the hairline gap between adjacent modules.
        // A trick borrowed from the MindMeld plugin:
        // tweak the clip box so we are allowed to draw 0.3 mm to the right of our own panel.
        newDrawArgs.clipBox.size.x += mm2px(DxRemoveGap);
        ModuleWidget::draw(newDrawArgs);
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
        {
            auto mw = dynamic_cast<ModuleWidget*>(w);
            if (mw && mw->module && mw->module->id == moduleId)
                return mw;
        }
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
        if (clone && parentModWidget->module)
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
        APP->engine->addModule(expanderModule);
        ModuleWidget* rawWidget = model->createModuleWidget(expanderModule);
        assert(rawWidget);
        SapphireWidget* sapphireWidget = dynamic_cast<SapphireWidget*>(rawWidget);
        assert(sapphireWidget);
        int dx = (dir == ExpanderDirection::Left) ? 0 : parentModWidget->box.size.x;
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
        bool hasPresets = false;
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
                    hasPresets = true;

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
                    hasPresets = true;
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
        if (!hasPresets)
        {
            menu->addChild(createMenuLabel(string::translate("ModuleWidget.nonePresets")));
        }
    };
}