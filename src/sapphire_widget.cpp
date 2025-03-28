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

    void SapphireWidget::drawLayer(const DrawArgs& args, int layer)
    {
        ModuleWidget::drawLayer(args, layer);
        if (layer == 1)
            drawSplash(args.vg);
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