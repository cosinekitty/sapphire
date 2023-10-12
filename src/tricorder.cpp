#include <vector>
#include "plugin.hpp"
#include "sapphire_widget.hpp"
#include "tricorder.hpp"

// Tricorder for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

namespace Sapphire
{
    namespace Tricorder
    {
        struct TricorderWidget;

        const int TRAIL_LENGTH = 2000;      // how many (x, y, z) points are held for the 3D plot

        struct Point
        {
            float x;
            float y;
            float z;

            Point(float _x, float _y, float _z)
                : x(_x)
                , y(_y)
                , z(_z)
                {}
        };

        using PointList = std::vector<Point>;

        enum ParamId
        {
            PARAMS_LEN
        };

        enum InputId
        {
            INPUTS_LEN
        };

        enum OutputId
        {
            OUTPUTS_LEN
        };

        enum LightId
        {
            LIGHTS_LEN
        };

        struct TricorderModule : Module
        {
            PointList pointList;
            int nextPointIndex{};
            float secondsAccum{};
            float xprev{};
            float yprev{};
            float zprev{};

            TricorderModule()
            {
                pointList.reserve(TRAIL_LENGTH);
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                initialize();
            }

            void resetPointList()
            {
                pointList.clear();
                nextPointIndex = 0;
                secondsAccum = 0.0f;
                xprev = yprev = zprev = 0.0f;
            }

            void initialize()
            {
                resetPointList();
            }

            void onReset(const ResetEvent& e) override
            {
                Module::onReset(e);
                initialize();
            }

            static bool isCompatibleModule(const Module *module)
            {
                if (module == nullptr)
                    return false;

                if (module->model == modelFrolic)
                    return true;

                return false;
            }

            const TricorderMessage* inboundMessage() const
            {
                if (isCompatibleModule(leftExpander.module))
                    return static_cast<const TricorderMessage *>(leftExpander.module->rightExpander.consumerMessage);

                return nullptr;
            }

            static float filter(float v)
            {
                if (!std::isfinite(v))
                    return 0.0f;

                return std::max(-10.0f, std::min(+10.0f, v));
            }

            void process(const ProcessArgs& args) override
            {
                // Is a compatible module connected to the left?
                // If so, receive a triplet of voltages from it and put them in the buffer.
                const TricorderMessage *msg = inboundMessage();
                if (msg == nullptr)
                {
                    // There is no compatible module flush to the left of Tricorder.
                    // Clear the display if it has stuff on it.
                    resetPointList();
                }
                else
                {
                    int n = static_cast<int>(pointList.size());

                    // Sanity check the values fed to us from the other module.
                    float x = filter(msg->x);
                    float y = filter(msg->y);
                    float z = filter(msg->z);
                    Point p(x, y, z);

                    // Only insert new points if the position has changed significantly
                    // or a sufficient amount of time has passed.
                    // But always insert the point if it is the first one!
                    secondsAccum += args.sampleTime;
                    float dx = x - xprev;
                    float dy = y - yprev;
                    float dz = z - zprev;
                    float distance = std::sqrt(dx*dx + dy*dy + dz*dz);

                    if (n == 0 || secondsAccum > 0.2f || distance > 0.05f)
                    {
                        if (n < TRAIL_LENGTH)
                        {
                            pointList.push_back(p);
                        }
                        else
                        {
                            pointList[nextPointIndex] = p;
                            nextPointIndex = (nextPointIndex + 1) % TRAIL_LENGTH;
                        }

                        // Because we added a point, reset the creteria for adding another point.
                        secondsAccum = 0.0f;
                        xprev = x;
                        yprev = y;
                        zprev = z;
                    }
                }
            }
        };


        struct TricorderDisplay : LedDisplay
        {
            float voltageScale = 5.0f;
            const float MM_SIZE = 105.0f;
            TricorderModule* module;
            TricorderWidget* parent;

            TricorderDisplay(TricorderModule* _module, TricorderWidget* _parent)
                : module(_module)
                , parent(_parent)
            {
                box.pos = mm2px(Vec(10.5f, 12.0f));
                box.size = mm2px(Vec(MM_SIZE, MM_SIZE));
            }

            void drawLayer(const DrawArgs& args, int layer) override
            {
                if (layer != 1)
                    return;

                drawBackground(args);

                if (module == nullptr)
                    return;

                const int n = static_cast<int>(module->pointList.size());
                if (n == 0)
                    return;

                nvgSave(args.vg);
                nvgStrokeColor(args.vg, SCHEME_YELLOW);
                Rect b = box.zeroPos();
                nvgScissor(args.vg, RECT_ARGS(b));
                nvgBeginPath(args.vg);
                if (n < TRAIL_LENGTH)
                {
                    // The pointList has not yet reached full capacity.
                    // Render from the front to the back.
                    for (int i = 1; i < n; ++i)
                        drawSegment(args.vg, i==1, module->pointList.at(i-1), module->pointList.at(i));

                    drawTip(args.vg, module->pointList.at(n-1));
                }
                else
                {
                    // The pointList is full, so we treat it as a circular buffer.
                    int curr = module->nextPointIndex;      // the oldest point in the list
                    for (int i = 1; i < TRAIL_LENGTH; ++i)
                    {
                        int next = (curr + 1) % TRAIL_LENGTH;
                        drawSegment(args.vg, i==1, module->pointList.at(curr), module->pointList.at(next));
                        curr = next;
                    }
                    drawTip(args.vg, module->pointList.at(curr));
                }
                nvgResetScissor(args.vg);
                nvgRestore(args.vg);
            }

            void drawSegment(NVGcontext* vg, bool first, const Point& a, const Point& b)
            {
                if (first)
                {
                    Vec sa = project(a);
                    nvgMoveTo(vg, sa.x, sa.y);
                }
                Vec sb = project(b);
                nvgLineTo(vg, sb.x, sb.y);
            }

            void drawTip(NVGcontext* vg, const Point& p)
            {
                nvgStroke(vg);
            }

            void drawBackground(const DrawArgs& args)
            {
            }

            Vec project(const Point& p)
            {
                // Project the 3D point 'p' onto a screen location Vec.
                float x = (MM_SIZE/2) * (1 + p.x/voltageScale);
                float y = (MM_SIZE/2) * (1 - p.y/voltageScale);
                return mm2px(Vec(x, y));
            }
        };


        struct TricorderWidget : SapphireReloadableModuleWidget
        {
            explicit TricorderWidget(TricorderModule *module)
                : SapphireReloadableModuleWidget(asset::plugin(pluginInstance, "res/tricorder.svg"))
            {
                setModule(module);

                // Load the SVG and place all controls at their correct coordinates.
                reloadPanel();

                addChild(new TricorderDisplay(module, this));
            }
        };
    }
}

Model *modelTricorder = createModel<Sapphire::Tricorder::TricorderModule, Sapphire::Tricorder::TricorderWidget>("Tricorder");
