#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"

namespace Sapphire
{
    void SapphireWidget::drawSplash(NVGcontext* vg)
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
                NVGcolor color = nvgRGBA(0xa5, 0x1f, 0xde, opacity);
                nvgBeginPath(vg);
                nvgRect(vg, 0, 0, box.size.x, box.size.y);
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


    void DrawBorders(NVGcontext* vg, const Rect& box, bool neon, bool hideLeft, bool hideRight)
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


    void DrawGlowingBorders(NVGcontext* vg, const Rect& box, bool hideLeft, bool hideRight)
    {
        const float margin = 1;
        const float vertical = box.size.y - 2*margin;
        const NVGcolor borderColor = nvgRGB(0x5d, 0x43, 0xa3);

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


    void SapphireWidget::draw(const DrawArgs& args)
    {
        DrawArgs newDrawArgs = args;

        // Eliminate the hairline gap between adjacent modules.
        // A trick borrowed from the MindMeld plugin:
        // tweak the clip box so we are allowed to draw 0.3 mm to the right of our own panel.
        newDrawArgs.clipBox.size.x += mm2px(DxRemoveGap);
        ModuleWidget::draw(newDrawArgs);
        DrawBorders(args.vg, box, isNeonModeActive(), isLeftBorderHidden(), isRightBorderHidden());
    }


    void SapphireWidget::drawLayer(const DrawArgs& args, int layer)
    {
        ModuleWidget::drawLayer(args, layer);
        if (layer == 1)
        {
            drawSplash(args.vg);
            if (isNeonModeActive())
                DrawGlowingBorders(args.vg, box, isLeftBorderHidden(), isRightBorderHidden());
        }
    }


    SapphireModule* AddExpander(Model* model, ModuleWidget* parentModWidget, ExpanderDirection dir)
    {
        Module* rawModule = model->createModule();
        assert(rawModule != nullptr);
        SapphireModule* expanderModule = dynamic_cast<SapphireModule*>(rawModule);
        assert(expanderModule != nullptr);
        APP->engine->addModule(expanderModule);
        ModuleWidget* rawWidget = model->createModuleWidget(expanderModule);
        assert(rawWidget != nullptr);
        SapphireWidget* sapphireWidget = dynamic_cast<SapphireWidget*>(rawWidget);
        assert(sapphireWidget != nullptr);
        int dx = (dir == ExpanderDirection::Left) ? -sapphireWidget->box.size.x : parentModWidget->box.size.x;
        APP->scene->rack->setModulePosForce(sapphireWidget, Vec{parentModWidget->box.pos.x + dx, parentModWidget->box.pos.y});
        APP->scene->rack->addModule(sapphireWidget);

        // Push this module creation action onto undo/redo stack.
        auto h = new history::ModuleAdd;
        h->name = "create " + model->name;
        h->setModule(sapphireWidget);
        APP->history->push(h);

        // Animate the first few frames of the new panel, like a splash screen.
        sapphireWidget->splash.begin();

        return expanderModule;
    }
}