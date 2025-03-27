#include "sapphire_vcvrack.hpp"
#include "sapphire_widget.hpp"

namespace Sapphire
{
    void SapphireWidget::beginSplash()
    {
        splashCount = 128;
    }

    void SapphireWidget::drawSplash(NVGcontext* vg)
    {
        if (splashCount > 0)
        {
            // Draw a box over the whole panel with gradually decreasing opacity.
            NVGcolor color = nvgRGBA(0xa5, 0x1f, 0xde, splashCount);
            nvgBeginPath(vg);
            nvgRect(vg, 0, 0, box.size.x, box.size.y);
            nvgFillColor(vg, color);
            nvgFill(vg);

            // Prepare for next frame of the animation.
            splashCount = std::max(0, splashCount - 8);
        }
    }

    void SapphireWidget::drawLayer(const DrawArgs& args, int layer)
    {
        ModuleWidget::drawLayer(args, layer);
        if (layer == 1)
            drawSplash(args.vg);
    }
}