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

    void SapphireWidget::eraseBorder(NVGcontext* vg, int side)
    {
        const NVGcolor sapphirePanelColor = nvgRGB(0x4f, 0x8d, 0xf2);

        // Draw a vertical line using the background panel color.
        const float xMargin = 1;
        const float yMargin = 1;
        float x1 = side*(box.size.x - xMargin);

        nvgBeginPath(vg);
        nvgRect(vg, x1, yMargin, xMargin + DxRemoveGap, box.size.y - 2*yMargin);
        nvgFillColor(vg, sapphirePanelColor);
        nvgFill(vg);
    }

    void SapphireWidget::updateBorders(NVGcontext* vg)
    {
        SapphireModule* smod = getSapphireModule();
        if (smod != nullptr)
        {
            if (smod->hideLeftBorder)
                eraseBorder(vg, 0);

            if (smod->hideRightBorder)
                eraseBorder(vg, 1);
        }
    }


    void SapphireWidget::draw(const DrawArgs& args)
    {
        DrawArgs newDrawArgs = args;

        if (isRightBorderHidden())
        {
            // Eliminate the hairline gap between adjacent modules in some cases.
            // A trick borrowed from the MindMeld plugin: when we want a seamless
            // connection between two adjacent panels (e.g. for expanders),
            // tweak the clip box so we are allowed to draw 0.3 mm to the right
            // of our own panel.
            newDrawArgs.clipBox.size.x += mm2px(DxRemoveGap);
        }

        if (isLeftBorderHidden())
        {
            newDrawArgs.clipBox.pos.x  -= mm2px(DxRemoveGap);
            newDrawArgs.clipBox.size.x += mm2px(DxRemoveGap);
        }

        ModuleWidget::draw(newDrawArgs);
    }


    void SapphireWidget::drawLayer(const DrawArgs& args, int layer)
    {
        ModuleWidget::drawLayer(args, layer);

        if (layer == 1)
        {
            updateBorders(args.vg);
            drawSplash(args.vg);
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