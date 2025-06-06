#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"

namespace Sapphire
{
    std::vector<SapphireModule*> SapphireModule::All;

    void SapphireWidget::ToggleAllNeonBorders()
    {
        // Vote: how many modules have neon mode enabled, and how many disabled?
        int brightCount = 0;
        int darkCount = 0;
        for (const SapphireModule* smod : SapphireModule::All)
            smod->neonMode ? ++brightCount : ++darkCount;

        if (brightCount + darkCount > 0)
        {
            // If more than half are enabled, turn all off.
            // Otherwise turn all on.
            const bool neon = (2*brightCount <= darkCount);
            for (SapphireModule* smod : SapphireModule::All)
                smod->neonMode = neon;
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
                auto parentModule = dynamic_cast<SapphireModule*>(parentModWidget->module);
                if (parentModule)
                    expanderModule->tryCopySettingsFrom(parentModule);
            }
        }
        APP->engine->addModule(expanderModule);
        ModuleWidget* rawWidget = model->createModuleWidget(expanderModule);
        assert(rawWidget);
        SapphireWidget* sapphireWidget = dynamic_cast<SapphireWidget*>(rawWidget);
        assert(sapphireWidget);
        int dx = (dir == ExpanderDirection::Left) ? -sapphireWidget->box.size.x : parentModWidget->box.size.x;
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
}