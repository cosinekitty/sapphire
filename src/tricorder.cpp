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

            Point()
                : x(0)
                , y(0)
                , z(0)
                {}

            Point(float _x, float _y, float _z)
                : x(_x)
                , y(_y)
                , z(_z)
                {}
        };

        using PointList = std::vector<Point>;

        class RotationMatrix
        {
        private:
            float rot[3][3];

        public:
            RotationMatrix()
            {
                initialize();
            }

            void initialize()
            {
                rot[0][0] = 1;
                rot[0][1] = 0;
                rot[0][2] = 0;
                rot[1][0] = 0;
                rot[1][1] = 1;
                rot[1][2] = 0;
                rot[2][0] = 0;
                rot[2][1] = 0;
                rot[2][2] = 1;
            }

            void pivot(
                unsigned axis,     // selects which axis to rotate around: 0=x, 1=y, 2=z
                float radians)     // rotation counterclockwise looking from the positive direction of `axis` toward the origin
            {
                float c = std::cos(radians);
                float s = std::sin(radians);

                // We need to maintain the "right-hand" rule, no matter which
                // axis was selected. That means we pick (i, j, k) axis order
                // such that the following vector cross product is satisfied:
                // i x j = k
                unsigned i = (axis + 1) % 3;
                unsigned j = (axis + 2) % 3;
                unsigned k = axis % 3;

                float t   = c*rot[i][i] - s*rot[i][j];
                rot[i][j] = s*rot[i][i] + c*rot[i][j];
                rot[i][i] = t;

                t         = c*rot[j][i] - s*rot[j][j];
                rot[j][j] = s*rot[j][i] + c*rot[j][j];
                rot[j][i] = t;

                t         = c*rot[k][i] - s*rot[k][j];
                rot[k][j] = s*rot[k][i] + c*rot[k][j];
                rot[k][i] = t;
            }

            Point rotate(const Point& p)
            {
                float x = rot[0][0]*p.x + rot[1][0]*p.y + rot[2][0]*p.z;
                float y = rot[0][1]*p.x + rot[1][1]*p.y + rot[2][1]*p.z;
                float z = rot[0][2]*p.x + rot[1][2]*p.y + rot[2][2]*p.z;
                return Point(x, y, z);
            }
        };

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
            int pointCount{};
            int nextPointIndex{};
            float secondsAccum{};
            float xprev{};
            float yprev{};
            float zprev{};

            TricorderModule()
            {
                pointList.resize(TRAIL_LENGTH);     // maintain fixed length for entire lifetime
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                initialize();
            }

            void resetPointList()
            {
                pointCount = 0;
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

            const Message* inboundMessage() const
            {
                if (isCompatibleModule(leftExpander.module))
                {
                    const Message* message = static_cast<const Message *>(leftExpander.module->rightExpander.consumerMessage);
                    if (IsValidMessage(message))
                        return message;
                }

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
                const Message *msg = inboundMessage();
                if (msg == nullptr)
                {
                    // There is no compatible module flush to the left of Tricorder.
                    // Clear the display if it has stuff on it.
                    resetPointList();
                }
                else
                {
                    // Sanity check the values fed to us from the other module.
                    float x = filter(msg->x);
                    float y = filter(msg->y);
                    float z = filter(msg->z);
                    Point p(x, y, z);

                    // Only insert new points if the position has changed significantly
                    // or a sufficient amount of time has passed.
                    secondsAccum += args.sampleTime;
                    float dx = x - xprev;
                    float dy = y - yprev;
                    float dz = z - zprev;
                    float distance = std::sqrt(dx*dx + dy*dy + dz*dz);

                    if (secondsAccum > 0.05f || distance > 0.05f)
                    {
                        if (pointCount < TRAIL_LENGTH)
                        {
                            pointList[pointCount++] = p;
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
            float radiansPerStep = -0.004f;
            float rotationRadians = 0.0f;
            float voltageScale = 5.0f;
            const float MM_SIZE = 105.0f;
            TricorderModule* module;
            TricorderWidget* parent;
            RotationMatrix orientation;

            TricorderDisplay(TricorderModule* _module, TricorderWidget* _parent)
                : module(_module)
                , parent(_parent)
            {
                box.pos = mm2px(Vec(10.5f, 12.0f));
                box.size = mm2px(Vec(MM_SIZE, MM_SIZE));
                orientation.pivot(0, 20.0*(M_PI/180.0));
            }

            NVGcolor trailColor(int i, int n)
            {
                float fadeDenom = TRAIL_LENGTH / 3.0f;
                NVGcolor tail = SCHEME_PURPLE;
                NVGcolor head = SCHEME_CYAN;
                tail.a = std::min(1.0f, i/fadeDenom);
                float rise = std::min(1.0f, (n-i)/fadeDenom);
                tail.r = rise*tail.r + (1-rise)*head.r;
                tail.g = rise*tail.g + (1-rise)*head.g;
                tail.b = rise*tail.b + (1-rise)*head.b;
                return tail;
            }

            void drawLayer(const DrawArgs& args, int layer) override
            {
                if (layer != 1)
                    return;

                if (module == nullptr)
                    return;

                drawBackground(args);

                const int n = module->pointCount;
                if (n == 0)
                    return;

                nvgSave(args.vg);
                Rect b = box.zeroPos();
                nvgScissor(args.vg, RECT_ARGS(b));

                if (n < TRAIL_LENGTH)
                {
                    // The pointList has not yet reached full capacity.
                    // Render from the front to the back.
                    for (int i = 1; i < n; ++i)
                    {
                        NVGcolor color = trailColor(i, n);
                        line(args.vg, color, module->pointList.at(i-1), module->pointList.at(i));
                    }
                    drawTip(args.vg, module->pointList.at(n-1));
                }
                else
                {
                    // The pointList is full, so we treat it as a circular buffer.
                    int curr = module->nextPointIndex;      // the oldest point in the list
                    for (int i = 1; i < TRAIL_LENGTH; ++i)
                    {
                        int next = (curr + 1) % TRAIL_LENGTH;
                        NVGcolor color = trailColor(i, n);
                        line(args.vg, color, module->pointList.at(curr), module->pointList.at(next));
                        curr = next;
                    }
                    drawTip(args.vg, module->pointList.at(curr));
                }
                nvgResetScissor(args.vg);
                nvgRestore(args.vg);
            }

            void line(NVGcontext *vg, const NVGcolor& color, const Point& a, const Point& b)
            {
                Vec sa = project(a);
                Vec sb = project(b);
                nvgBeginPath(vg);
                nvgStrokeColor(vg, color);
                nvgMoveTo(vg, sa.x, sa.y);
                nvgLineTo(vg, sb.x, sb.y);
                nvgStroke(vg);
            }

            void drawTip(NVGcontext* vg, const Point& p)
            {
                Vec s = project(p);
                nvgBeginPath(vg);
                nvgStrokeColor(vg, SCHEME_WHITE);
                nvgFillColor(vg, SCHEME_WHITE);
                nvgCircle(vg, s.x, s.y, 1.0);
                nvgFill(vg);
            }

            void drawAxis(NVGcontext* vg, char label, Point tip, Point arrow1, Point arrow2)
            {
                Point origin(0, 0, 0);
                NVGcolor axisColor = nvgRGB(0x60, 0x60, 0x60);
                line(vg, axisColor, origin, tip);
                line(vg, axisColor, tip, arrow1);
                line(vg, axisColor, tip, arrow2);
            }

            void drawBackground(const DrawArgs& args)
            {
                const float r = 4.0f;
                const float a = 0.93f * r;
                const float b = 0.03f * r;
                drawAxis(args.vg, 'X', Point(r, 0, 0), Point(a, +b, 0), Point(a, -b, 0));
                drawAxis(args.vg, 'Y', Point(0, r, 0), Point(+b, a, 0), Point(-b, a, 0));
                drawAxis(args.vg, 'Z', Point(0, 0, r), Point(0, +b, a), Point(0, -b, a));
            }

            Vec project(const Point& p)
            {
                // Apply the rotation matrix to the 3D point.
                Point q = orientation.rotate(p);

                // Project the 3D point 'p' onto a screen location Vec.
                float sx = (MM_SIZE/2) * (1 + q.x/voltageScale);
                float sy = (MM_SIZE/2) * (1 - q.y/voltageScale);
                return mm2px(Vec(sx, sy));
            }

            void step() override
            {
                rotationRadians = std::fmod(rotationRadians + radiansPerStep, 2*M_PI);
                orientation.initialize();
                orientation.pivot(1, rotationRadians);
                orientation.pivot(0, 20*(M_PI/180));
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
