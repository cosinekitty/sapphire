#include <algorithm>
#include <vector>
#include "plugin.hpp"
#include "sapphire_widget.hpp"

// Tricorder for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

namespace Sapphire
{
    namespace Tricorder
    {
        struct TricorderWidget;
        struct TricorderDisplay;

        const int TRAIL_LENGTH = 1000;      // how many (x, y, z) points are held for the 3D plot

        const int PANEL_HP_WIDTH = 25;
        const float HP_MM = 5.08f;
        const float PANEL_MM_WIDTH = PANEL_HP_WIDTH * HP_MM;
        const float PANEL_MM_HEIGHT = 128.5f;

        const float DISPLAY_MM_MARGIN = 3.0f;
        const float DISPLAY_MM_WIDTH  = PANEL_MM_WIDTH - (2*DISPLAY_MM_MARGIN);
        const float DISPLAY_MM_HEIGHT = 114.0f;
        const float DISPLAY_SCALE     = std::min(DISPLAY_MM_WIDTH, DISPLAY_MM_HEIGHT);
        const float BUTTON_WIDTH      = 0.10f * DISPLAY_SCALE;
        const float BUTTON_HEIGHT     = 0.10f * DISPLAY_SCALE;
        const float BUTTON_LEFT       = 0.05f * DISPLAY_MM_WIDTH;
        const float BUTTON_LEFTCENTER = 0.25f * DISPLAY_MM_WIDTH;
        const float BUTTON_RIGHT      = 0.85f * DISPLAY_MM_WIDTH;
        const float BUTTON_CENTER     = 0.45f * DISPLAY_MM_WIDTH;
        const float BUTTON_TOP        = 0.05f * DISPLAY_MM_HEIGHT;
        const float BUTTON_BOTTOM     = 0.85f * DISPLAY_MM_HEIGHT;
        const float BUTTON_MIDDLE     = 0.45f * DISPLAY_MM_HEIGHT;

        const float NUMERIC_TOP       = 0.98f * DISPLAY_MM_HEIGHT;
        const float NUMERIC_X_LEFT    = 0.10f * DISPLAY_MM_WIDTH;
        const float NUMERIC_Y_LEFT    = 0.40f * DISPLAY_MM_WIDTH;
        const float NUMERIC_Z_LEFT    = 0.70f * DISPLAY_MM_WIDTH;

        const float MIN_RPM = 0.01;
        const float MAX_RPM = 100;
        const float DEFAULT_RPM = std::sqrt(MIN_RPM * MAX_RPM);

        enum class MousePosition
        {
            Absent,         // the mouse is not inside a button nor inside the display rectangle
            InDisplay,      // the mouse is inside the display after enough step() ticks
            InButton,       // the mouse is inside a button after enough step() ticks
        };


        class MouseStateManager     // helps "denoise" state transitions for mouse ownership
        {
        private:
            MousePosition currPos = MousePosition::Absent;
            MousePosition nextPos = MousePosition::Absent;
            unsigned count = 0;
            const unsigned threshold = 3;       // how many step() calls before we consider mouse ownership changed?

        public:
            MousePosition position() const
            {
                return currPos;
            }

            void step()
            {
                // Debounce changes in state, so that we ignore a series of rapid changes.
                if (nextPos != currPos)
                {
                    if (count < threshold)
                    {
                        ++count;
                    }
                    else
                    {
                        count = 0;
                        currPos = nextPos;
                    }
                }
            }

            void requestNextPos(MousePosition pos)
            {
                if (pos != nextPos)
                {
                    nextPos = pos;
                    count = 0;
                }
            }

            void onEnterDisplayArea()
            {
                requestNextPos(MousePosition::InDisplay);
            }

            void onLeaveDisplayArea()
            {
                requestNextPos(MousePosition::Absent);
            }

            void onEnterButton()
            {
                requestNextPos(MousePosition::InButton);
            }

            void onLeaveButton()
            {
                requestNextPos(MousePosition::Absent);
            }
        };


        class ButtonFader    // manages when Tricorder display buttons are hidden/waxing/opaque/fading
        {
        private:
            bool fading = true;
            unsigned count = 0;
            unsigned loThreshold = 0;
            unsigned hiThreshold = 1;
            unsigned stillCount = 0;
            const unsigned stillLoThresh = 200;
            const unsigned stillHiThresh = 400;
            MouseStateManager mstate;

        public:
            void step()
            {
                const MousePosition prevPosition = mstate.position();
                mstate.step();
                const MousePosition currPosition = mstate.position();

                if (currPosition == prevPosition)
                {
                    // We are still in the same state.
                    // Some states have faders that require sliding a value over time.
                    if (count < hiThreshold)
                        ++count;
                }
                else
                {
                    // The state just changed. What do we need to (re-)initialize?
                    switch (currPosition)
                    {
                    case MousePosition::InDisplay:
                        // If we were just inside the InButton state, stay fully visible.
                        // Otherwise, prepare to fade in.
                        loThreshold = 20;
                        hiThreshold = 60;
                        fading = false;
                        count = (prevPosition == MousePosition::InButton) ? hiThreshold : 0;
                        break;

                    case MousePosition::InButton:
                        // Go instantly to the in-button state.
                        count = 1;
                        loThreshold = 0;
                        hiThreshold = 1;
                        fading = false;
                        break;

                    case MousePosition::Absent:
                    default:
                        // Elvis has left the building. Begin fading out immediately.
                        count = 0;
                        loThreshold = 0;
                        hiThreshold = 50;
                        fading = true;
                        break;
                    }
                }
            }

            void onMouseStill()
            {
                if (mstate.position() == MousePosition::InDisplay)
                {
                    if (stillCount < stillHiThresh)
                        ++stillCount;
                }
                else
                    stillCount = 0;
            }

            void onMouseMove()
            {
                // Any time the mouse moves, cancel any fading that occurred due to being stationary for too long.
                stillCount = 0;
            }

            float fade() const
            {
                float factor;

                // Check for too much mouse stillness while inside the display area (but outside all buttons).
                if (mstate.position() == MousePosition::InDisplay)
                    if (stillCount >= stillLoThresh && stillHiThresh > stillLoThresh)
                        return 1 - static_cast<float>(stillCount - stillLoThresh) / (stillHiThresh - stillLoThresh);

                if (count >= loThreshold && hiThreshold > loThreshold)
                    factor = static_cast<float>(count - loThreshold) / (hiThreshold - loThreshold);
                else
                    factor = 0;

                if (fading)
                    factor = 1 - factor;

                return factor;
            }

            bool areButtonsVisible() const
            {
                return fade() > 0;
            }

            void onEnterDisplayArea()
            {
                mstate.onEnterDisplayArea();
            }

            void onLeaveDisplayArea()
            {
                mstate.onLeaveDisplayArea();
            }

            void onEnterButton()
            {
                mstate.onEnterButton();
            }

            void onLeaveButton()
            {
                mstate.onLeaveButton();
            }
        };


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

            Point rotate(const Point& p) const
            {
                float x = rot[0][0]*p.x + rot[1][0]*p.y + rot[2][0]*p.z;
                float y = rot[0][1]*p.x + rot[1][1]*p.y + rot[2][1]*p.z;
                float z = rot[0][2]*p.x + rot[1][2]*p.y + rot[2][2]*p.z;
                return Point(x, y, z);
            }
        };

        inline float WrapAngle(float radians)
        {
            float wrap = std::fmod(radians, 2*M_PI);
            if (wrap < 0)
                wrap += 2*M_PI;
            return wrap;
        }

        enum ParamId
        {
            ROTATION_SPEED_PARAM,

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

        struct RotationSpeedQuantity : SapphireQuantity
        {
            void setDisplayValue(float displayValue) override
            {
                // Inverse function of getDisplayValue().
                // Given the RPM range [MIN_RPM, MAX_RPM], map onto the
                // logarithmic range [-1, +1].
                // displayValue = MIN_RPM * std::pow(MAX_RPM/MIN_RPM, (value + 1)/2)
                // Solve for value:
                // log(displayValue / MIN_RPM)/log(MAX_RPM/MIN_RPM) = (value + 1)/2
                // value = 2*log(displayValue / MIN_RPM)/log(MAX_RPM/MIN_RPM) - 1
                float v = 2*std::log(displayValue / MIN_RPM)/std::log(MAX_RPM / MIN_RPM) - 1;
                setValue(v);
            }

            float getDisplayValue() override
            {
                // The knob's raw range is [-1, +1].
                // We map this to an exponential range from [MIN_RPM, MAX_RPM].
                float v = getValue();
                return MIN_RPM * std::pow(MAX_RPM/MIN_RPM, (v+1)/2);
            }

            std::string getDisplayValueString() override
            {
                float rpm = getDisplayValue();
                return rack::string::f("%0.2f RPM", rpm);
            }
        };

        struct TricorderModule : SapphireModule
        {
            PointList pointList;
            int pointCount{};
            int nextPointIndex{};
            float xprev{};
            float yprev{};
            float zprev{};
            float xcurr{};
            float ycurr{};
            float zcurr{};
            bool bypassing = false;
            RotationSpeedQuantity* rotationSpeedQuantity{};
            float yRotationRadians{};
            float xRotationRadians{};
            int londir{};
            int latdir{};
            bool axesAreVisible{};
            bool numbersAreVisible{};
            RotationMatrix orientation;
            const float defaultVoltageScale = 5.0f;
            float voltageScale{};
            mutable Stopwatch stopwatch;    // used for directly measuring actual VCV Rack frame rates

            TricorderModule()
                : SapphireModule(PARAMS_LEN)
            {
                pointList.resize(TRAIL_LENGTH);     // maintain fixed length for entire lifetime
                config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
                makeRotationQuantity();
                initialize();
            }

            void makeRotationQuantity()
            {
                rotationSpeedQuantity = configParam<RotationSpeedQuantity>(
                    ROTATION_SPEED_PARAM,
                    -1,
                    +1,
                    0,
                    "Rotation speed"
                );
                rotationSpeedQuantity->value = 0;
            }

            void resetPointList()
            {
                pointCount = 0;
                nextPointIndex = 0;
                xprev = yprev = zprev = 0.0f;
            }

            void initialize()
            {
                axesAreVisible = true;
                numbersAreVisible = false;
                resetPointList();
                resetPerspective();
                setRotationSpeed();
                selectRotationMode(-1, 0);
                stopwatch.reset();
            }

            void resetPerspective()
            {
                voltageScale = defaultVoltageScale;
                yRotationRadians = -11.0*(M_PI/180);
                xRotationRadians = +23.5*(M_PI/180);
            }

            float zoomFactor() const
            {
                return voltageScale / defaultVoltageScale;
            }

            void adjustZoom(int adjust)
            {
                const float factor = 1.1f;
                const float maxVoltageScale = 20.0f;
                const float minVoltageScale = 0.1f;
                float s;
                if (adjust < 0)
                {
                    // Zooming out, so make voltage scale larger.
                    s = voltageScale * factor;
                }
                else
                {
                    // Zooming in, so make voltage scale smaller.
                    s = voltageScale / factor;
                }

                if (minVoltageScale <= s && s <= maxVoltageScale)
                    voltageScale = s;
            }

            void onReset(const ResetEvent& e) override
            {
                Module::onReset(e);
                initialize();
            }

            void onBypass(const BypassEvent&) override
            {
                bypassing = true;
                resetPointList();
            }

            void onUnBypass(const UnBypassEvent&) override
            {
                bypassing = false;
            }

            static inline float filter(float v)
            {
                return std::isfinite(v) ? v : 0;
            }

            void process(const ProcessArgs& args) override
            {
                // Is a compatible module connected to the left?
                // If so, receive a triplet of voltages from it and put them in the buffer.
                Message *msg = vectorReceiver.inboundVectorMessage();
                if (msg == nullptr)
                {
                    // There is no compatible module flush to the left of Tricorder.
                    // Clear the display if it has stuff on it.
                    resetPointList();
                }
                else
                {
                    if (msg->isResetRequested())
                    {
                        // The sender is telling us that it would be a good idea to start over
                        // with an empty trail.
                        resetPointList();
                    }

                    // Sanity check the values fed to us from the other module.
                    xcurr = filter(msg->x);
                    ycurr = filter(msg->y);
                    zcurr = filter(msg->z);

                    // Daisy chain this inbound message from the left to any chained module on the right.
                    vectorSender.sendVector(xcurr, ycurr, zcurr, msg->isResetRequested());

                    // Only insert new points if the position has changed significantly
                    // or a sufficient amount of time has passed.
                    Point p(xcurr, ycurr, zcurr);
                    float dx = xcurr - xprev;
                    float dy = ycurr - yprev;
                    float dz = zcurr - zprev;
                    float distance = std::sqrt(dx*dx + dy*dy + dz*dz);

                    if (distance > 0.1f || pointCount == 0)
                    {
                        if (pointCount < TRAIL_LENGTH)
                        {
                            // We are still filling up the list.
                            pointList[pointCount++] = p;
                        }
                        else
                        {
                            // The list is full, so replace the oldest point with this new point.
                            pointList[nextPointIndex] = p;
                            nextPointIndex = (nextPointIndex + 1) % TRAIL_LENGTH;
                        }

                        // Because we added a point, reset the creteria for adding another point.
                        xprev = xcurr;
                        yprev = ycurr;
                        zprev = zcurr;
                    }
                    else    // pointCount > 0
                    {
                        // Instead of adding a new point, update the position of the most recently
                        // added point. This makes the animation much smoother.
                        if (pointCount < TRAIL_LENGTH)
                        {
                            // We were still filling up the list.
                            pointList[pointCount-1] = p;
                        }
                        else
                        {
                            // This list is full, so replace the oldest point with this new point.
                            int latestPointIndex = (nextPointIndex + (TRAIL_LENGTH - 1)) % TRAIL_LENGTH;
                            pointList[latestPointIndex] = p;
                        }
                    }
                }
            }

            void setRotationSpeed(float rpm = DEFAULT_RPM)
            {
                rotationSpeedQuantity->setDisplayValue(clamp(rpm, MIN_RPM, MAX_RPM));
            }

            float getRotationRadiansPerStep() const
            {
                // Convert the desired rotation speed expressed in revolutions per minute (RPM)
                // into radians per step. The step interval is itself variable.
                // We measure real time to calculate how many steps per minute are actually ocurring.
                // This number will change over time as computer loading varies.
                double secondsPerStep = stopwatch.restart();
                if (secondsPerStep <= 0)
                    return 0;   // it's OK to halt rotation a frame or two, until we know how fast it should be going.

                float stepsPerMinute = 60 / secondsPerStep;
                float radiansPerMinute = (2 * M_PI) * rotationSpeedQuantity->getDisplayValue();
                float radiansPerStep = radiansPerMinute / stepsPerMinute;
                return radiansPerStep;
            }

            void selectRotationMode(int longitudeDirection, int latitudeDirection)
            {
                londir = longitudeDirection;
                latdir = latitudeDirection;
            }

            void updateOrientation(float latChange, float lonChange)
            {
                yRotationRadians = WrapAngle(yRotationRadians + lonChange);
                xRotationRadians = WrapAngle(xRotationRadians + latChange);
                orientation.initialize();
                orientation.pivot(1, yRotationRadians);
                orientation.pivot(0, xRotationRadians);
            }

            void stepOrientation()
            {
                float rotationRadiansPerStep = getRotationRadiansPerStep();
                updateOrientation(latdir * rotationRadiansPerStep, londir * rotationRadiansPerStep);
            }

            json_t* dataToJson() override
            {
                // Save display settings into the json object.
                json_t* root = SapphireModule::dataToJson();

                // Save the current auto-rotation state.
                json_t* rotmode = json_array();
                json_array_append_new(rotmode, json_integer(latdir));
                json_array_append_new(rotmode, json_integer(londir));
                json_object_set_new(root, "rotation", rotmode);

                // Save the current angles of rotation about the x-axis and y-axis.
                json_t* orient = json_array();
                json_array_append_new(orient, json_real(xRotationRadians));
                json_array_append_new(orient, json_real(yRotationRadians));
                json_object_set_new(root, "orientation", orient);

                // Save the user-selected rotation speed expressed in RPM.
                json_object_set_new(root, "rotationSpeedRpm", json_real(rotationSpeedQuantity->getDisplayValue()));

                // Save the XYZ axes visibility state.
                json_object_set_new(root, "axesVisible", json_boolean(axesAreVisible));
                json_object_set_new(root, "numbersVisible", json_boolean(numbersAreVisible));

                // Save the zoom level (voltage scale).
                json_object_set_new(root, "voltageScale", json_real(voltageScale));

                return root;
            }

            void dataFromJson(json_t* root) override
            {
                SapphireModule::dataFromJson(root);

                // Restore display settings from the json object.
                json_t *rotmode = json_object_get(root, "rotation");
                if (json_is_array(rotmode) && json_array_size(rotmode) == 2)
                {
                    int initLatDir = json_integer_value(json_array_get(rotmode, 0));
                    int initLonDir = json_integer_value(json_array_get(rotmode, 1));
                    selectRotationMode(initLonDir, initLatDir);
                }

                json_t* rpm = json_object_get(root, "rotationSpeedRpm");
                if (json_is_number(rpm))
                {
                    double speed = json_number_value(rpm);
                    setRotationSpeed(static_cast<float>(speed));
                }

                json_t* orient = json_object_get(root, "orientation");
                if (json_is_array(orient) && json_array_size(orient) == 2)
                {
                    xRotationRadians = json_number_value(json_array_get(orient, 0));
                    yRotationRadians = json_number_value(json_array_get(orient, 1));
                }

                json_t* axesvis = json_object_get(root, "axesVisible");
                axesAreVisible = !json_is_false(axesvis);

                json_t* numvis = json_object_get(root, "numbersVisible");
                numbersAreVisible = !json_is_false(numvis);

                json_t* scale = json_object_get(root, "voltageScale");
                if (json_is_number(scale))
                    voltageScale = json_number_value(scale);
            }
        };


        int ActivePointCount(const TricorderModule* module)
        {
            if (module == nullptr)
                return 0;

            if (module->bypassing)
                return 0;

            return module->pointCount;
        }


        enum class SegmentKind
        {
            Curve,
            Axis,
            Tip,
        };


        struct LineSegment
        {
            Vec vec1;           // rotated and scaled screen coordinates for first endpoint
            Vec vec2;           // rotated and scaled screen coordinates for second endpoint
            float prox;         // z-order proximity of segment's midpoint: larger values are closer to the viewer
            SegmentKind kind;   // what coloring rules to apply to the segment
            int index;          // index along the trail, used for fading the end; [1 .. TRAIL_LENGTH-1]

            LineSegment(Vec _vec1, Vec _vec2, float _prox, SegmentKind _kind, int _index)
                : vec1(_vec1)
                , vec2(_vec2)
                , prox(_prox)
                , kind(_kind)
                , index(_index)
                {}

            static LineSegment MakeTip(Vec vec, float prox)   // total hack!
            {
                return LineSegment(vec, vec, prox, SegmentKind::Tip, -1);
            }
        };

        inline bool operator < (const LineSegment& a, const LineSegment& b)      // needed for sorting renderList
        {
            // We want to sort the renderList in ascending order of the `prox` field.
            // The larger `prox` is, the closer the midpoint of the linesegment is to the viewer.
            // We want to draw farther line segments first, closer line segments last, so that
            // the closer line segments overlap the farther ones.
            return a.prox < b.prox;
        }

        using RenderList = std::vector<LineSegment>;

        bool AreButtonsVisible(const TricorderDisplay&);
        float ButtonFade(const TricorderDisplay&);
        void ResetPerspective(const TricorderDisplay&);
        void SelectRotationMode(const TricorderDisplay&, int longitudeDirection, int latitudeDirection);
        void ToggleAxisVisibility(const TricorderDisplay&);
        void ToggleNumbersVisibility(const TricorderDisplay&);
        bool AxesAreVisible(const TricorderDisplay&);
        bool NumbersAreVisible(const TricorderDisplay& display);
        void AdjustZoom(const TricorderDisplay&, int adjust);
        void OnEnterDisplayButton(TricorderDisplay&);
        void OnLeaveDisplayButton(TricorderDisplay&);
        void ClearPath(TricorderDisplay&);

        struct TricorderButton : OpaqueWidget       // a mouse click target that appears when hovering over the TricorderDisplay
        {
            TricorderDisplay& display;
            bool ownsMouse = false;
            bool isMousePressed = false;
            float fade{};

            TricorderButton(TricorderDisplay& _display, float x1, float y1)
                : display(_display)
            {
                box.pos = mm2px(Vec(x1, y1));
                box.size = mm2px(Vec(BUTTON_WIDTH, BUTTON_HEIGHT));
            }

            bool isButtonVisible() const
            {
                // My own replacement for Widget::isVisible() that works around its issues
                // with supporting automatic hide/show of all buttons depending on whether
                // the mouse is inside the parent rectangle. The weird part comes from trying
                // to keep buttons visible when the mouse moves into a button, thus "leaving"
                // the parent window, causing the buttons to become invisible. This becomes
                // a vicious loop of showing/hiding all the buttons. My workaround resolves
                // this by keeping all buttons visible when the mouse cursor is owned by the parent
                // or any of the buttons inside the parent.
                return AreButtonsVisible(display);
            }

            void drawLayer(const DrawArgs& args, int layer) override
            {
                if (layer==1 && isButtonVisible())
                {
                    fade = ButtonFade(display);
                    NVGcolor color = nvgRGB(0x70, 0x58, 0x13);
                    color.a = (ownsMouse ? 1.0f : 0.2f) * fade;
                    math::Rect r = box.zeroPos();
                    nvgBeginPath(args.vg);
                    nvgRect(args.vg, RECT_ARGS(r));
                    nvgFillColor(args.vg, color);
                    nvgFill(args.vg);
                    OpaqueWidget::draw(args);
                }
            }

            void line(const DrawArgs& args, float x1, float y1, float x2, float y2)
            {
                NVGcolor color = ownsMouse ? SCHEME_WHITE : SCHEME_YELLOW;
                color.a = fade;
                nvgBeginPath(args.vg);
                nvgStrokeColor(args.vg, color);
                nvgLineCap(args.vg, NVG_ROUND);
                nvgMoveTo(args.vg, mm2px(BUTTON_WIDTH * x1), mm2px(BUTTON_HEIGHT * y1));
                nvgLineTo(args.vg, mm2px(BUTTON_WIDTH * x2), mm2px(BUTTON_HEIGHT * y2));
                nvgStroke(args.vg);
            }

            void onEnter(const EnterEvent& e) override
            {
                ownsMouse = true;
                OnEnterDisplayButton(display);
            }

            void onLeave(const LeaveEvent& e) override
            {
                ownsMouse = false;
                OnLeaveDisplayButton(display);
            }

            void onButton(const ButtonEvent& e) override
            {
                if (e.button == GLFW_MOUSE_BUTTON_LEFT)
                {
                    switch (e.action)
                    {
                    case GLFW_PRESS:    onMousePress();     break;
                    case GLFW_RELEASE:  onMouseRelease();   break;
                    }
                    e.consume(this);
                }
            }

            virtual void onButtonClick() {}

            virtual void onMousePress()
            {
                isMousePressed = true;
            }

            virtual void onMouseRelease()
            {
                if (isMousePressed)
                    onButtonClick();

                isMousePressed = false;
            }
        };


        struct TricorderButton_ToggleAxes : TricorderButton
        {
            explicit TricorderButton_ToggleAxes(TricorderDisplay& _display)
                : TricorderButton(_display, BUTTON_LEFT, BUTTON_BOTTOM)
                {}

            void onButtonClick() override
            {
                ToggleAxisVisibility(display);
            }

            void drawLayer(const DrawArgs& args, int layer) override
            {
                TricorderButton::drawLayer(args, layer);
                if (layer==1 && isButtonVisible())
                {
                    // The letter X
                    line(args, 0.1, 0.35, 0.3, 0.65);
                    line(args, 0.1, 0.65, 0.3, 0.35);

                    // The letter Y
                    line(args, 0.5, 0.65, 0.5, 0.5);
                    line(args, 0.5, 0.5, 0.4, 0.35);
                    line(args, 0.5, 0.5, 0.6, 0.35);

                    // The letter Z
                    line(args, 0.9, 0.65, 0.7, 0.65);
                    line(args, 0.7, 0.65, 0.9, 0.35);
                    line(args, 0.7, 0.35, 0.9, 0.35);
                }
            }
        };


        struct TricorderButton_ToggleNumbers : TricorderButton
        {
            explicit TricorderButton_ToggleNumbers(TricorderDisplay& _display)
                : TricorderButton(_display, BUTTON_LEFTCENTER, BUTTON_BOTTOM)
                {}

            void onButtonClick() override
            {
                ToggleNumbersVisibility(display);
            }

            void drawLayer(const DrawArgs& args, int layer) override
            {
                TricorderButton::drawLayer(args, layer);
                if (layer==1 && isButtonVisible())
                {
                    float dx = -0.04;

                    // The numeral 1
                    line(args, 0.2+dx, 0.35, 0.2+dx, 0.65);

                    // The numeral 2
                    float ex = dx - 0.04;
                    line(args, 0.4+ex, 0.35, 0.6+ex, 0.35);
                    line(args, 0.6+ex, 0.35, 0.6+ex, 0.50);
                    line(args, 0.6+ex, 0.50, 0.4+ex, 0.50);
                    line(args, 0.4+ex, 0.50, 0.4+ex, 0.65);
                    line(args, 0.4+ex, 0.65, 0.6+ex, 0.65);

                    // The numeral 3
                    line(args, 0.7+dx, 0.65, 0.9+dx, 0.65);
                    line(args, 0.9+dx, 0.65, 0.9+dx, 0.35);
                    line(args, 0.9+dx, 0.35, 0.7+dx, 0.35);
                    line(args, 0.9+dx, 0.50, 0.7+dx, 0.50);
                }
            }
        };


        struct TricorderSpinButton : TricorderButton
        {
            const bool flip;
            const bool swap;

            TricorderSpinButton(TricorderDisplay& _display, float x1, float y1, bool _flip, bool _swap)
                : TricorderButton(_display, x1, y1)
                , flip(_flip)
                , swap(_swap)
                {}

            void drawLayer(const DrawArgs& args, int layer) override
            {
                TricorderButton::drawLayer(args, layer);
                if (layer==1 && isButtonVisible())
                {
                    // Draw a double-chevron pattern, oriented in one of the four possible ways.
                    xline(args, 0.2, 0.8, 0.5, 0.6);
                    xline(args, 0.5, 0.6, 0.8, 0.8);
                    xline(args, 0.2, 0.4, 0.5, 0.2);
                    xline(args, 0.5, 0.2, 0.8, 0.4);
                }
            }

            void xline(const DrawArgs& args, float x1, float y1, float x2, float y2)
            {
                if (flip)
                {
                    y1 = 1 - y1;
                    y2 = 1 - y2;
                }
                if (swap)
                {
                    std::swap(x1, y1);
                    std::swap(x2, y2);
                }
                line(args, x1, y1, x2, y2);
            }
        };


        struct TricorderButton_SpinRight : TricorderSpinButton
        {
            explicit TricorderButton_SpinRight(TricorderDisplay& _display)
                : TricorderSpinButton(_display, BUTTON_RIGHT, BUTTON_MIDDLE, true, true)
                {}

            void onButtonClick() override
            {
                SelectRotationMode(display, +1, 0);
            }
        };


        struct TricorderButton_SpinLeft : TricorderSpinButton
        {
            explicit TricorderButton_SpinLeft(TricorderDisplay& _display)
                : TricorderSpinButton(_display, BUTTON_LEFT, BUTTON_MIDDLE, false, true)
                {}

            void onButtonClick() override
            {
                SelectRotationMode(display, -1, 0);
            }
        };


        struct TricorderButton_SpinUp : TricorderSpinButton
        {
            explicit TricorderButton_SpinUp(TricorderDisplay& _display)
                : TricorderSpinButton(_display, BUTTON_CENTER, BUTTON_TOP, false, false)
                {}

            void onButtonClick() override
            {
                SelectRotationMode(display, 0, -1);
            }
        };


        struct TricorderButton_SpinDown : TricorderSpinButton
        {
            explicit TricorderButton_SpinDown(TricorderDisplay& _display)
                : TricorderSpinButton(_display, BUTTON_CENTER, BUTTON_BOTTOM, true, false)
                {}

            void onButtonClick() override
            {
                SelectRotationMode(display, 0, +1);
            }
        };


        struct TricorderButton_Home : TricorderButton
        {
            explicit TricorderButton_Home(TricorderDisplay& _display)
                : TricorderButton(_display, BUTTON_LEFT, BUTTON_TOP)
                {}

            void onButtonClick() override
            {
                ResetPerspective(display);
            }

            void drawLayer(const DrawArgs& args, int layer) override
            {
                TricorderButton::drawLayer(args, layer);
                if (layer==1 && isButtonVisible())
                {
                    // Draw a little cartoon house.
                    line(args, 0.2, 0.5, 0.5, 0.3);
                    line(args, 0.5, 0.3, 0.8, 0.5);
                    line(args, 0.8, 0.5, 0.75, 0.5);
                    line(args, 0.75, 0.5, 0.75, 0.8);
                    line(args, 0.75, 0.8, 0.25, 0.8);
                    line(args, 0.25, 0.8, 0.25, 0.5);
                    line(args, 0.25, 0.5, 0.2, 0.5);
                }
            }
        };


        struct TricorderButton_ZoomIn : TricorderButton
        {
            explicit TricorderButton_ZoomIn(TricorderDisplay& _display)
                : TricorderButton(_display, BUTTON_RIGHT, BUTTON_TOP)
                {}

            void onButtonClick() override
            {
                AdjustZoom(display, +1);
            }

            void drawLayer(const DrawArgs& args, int layer) override
            {
                TricorderButton::drawLayer(args, layer);
                if (layer==1 && isButtonVisible())
                {
                    // Draw a plus sign.
                    line(args, 0.2, 0.5, 0.8, 0.5);
                    line(args, 0.5, 0.2, 0.5, 0.8);
                }
            }
        };


        struct TricorderButton_ZoomOut : TricorderButton
        {
            explicit TricorderButton_ZoomOut(TricorderDisplay& _display)
                : TricorderButton(_display, BUTTON_RIGHT, BUTTON_BOTTOM)
                {}

            void onButtonClick() override
            {
                AdjustZoom(display, -1);
            }

            void drawLayer(const DrawArgs& args, int layer) override
            {
                TricorderButton::drawLayer(args, layer);
                if (layer==1 && isButtonVisible())
                {
                    // Draw a minus sign.
                    line(args, 0.2, 0.5, 0.8, 0.5);
                }
            }
        };

        struct TricorderButton_ClearPath : TricorderButton
        {
            explicit TricorderButton_ClearPath(TricorderDisplay& _display)
                : TricorderButton(_display, BUTTON_LEFTCENTER, BUTTON_TOP)
                {}

            void onButtonClick() override
            {
                ClearPath(display);
            }

            void drawLayer(const DrawArgs& args, int layer) override
            {
                TricorderButton::drawLayer(args, layer);
                if (layer==1 && isButtonVisible())
                {
                    // Empty square brackets: []
                    line(args, 0.2, 0.2, 0.3, 0.2);
                    line(args, 0.2, 0.2, 0.2, 0.8);
                    line(args, 0.2, 0.8, 0.3, 0.8);
                    line(args, 0.8, 0.2, 0.7, 0.2);
                    line(args, 0.8, 0.2, 0.8, 0.8);
                    line(args, 0.8, 0.8, 0.7, 0.8);
                }
            }
        };

        struct TricorderDisplay : OpaqueWidget
        {
            TricorderModule* module;
            RenderList renderList;
            std::vector<TricorderButton*> buttonList;
            bool ownsMouse = false;
            bool isDragging = false;
            ButtonFader buttonFader;
            Vec hoverMousePos;      // valid only when ownsMouse is true
            std::string fontPath;
            float xprev{};
            float yprev{};
            float zprev{};

            explicit TricorderDisplay(TricorderModule* _module)
                : module(_module)
                , fontPath(asset::system("res/fonts/ShareTechMono-Regular.ttf"))
            {
                const float mmTopMargin  = PANEL_MM_HEIGHT - (DISPLAY_MM_HEIGHT + DISPLAY_MM_MARGIN);
                box.pos = mm2px(Vec(DISPLAY_MM_MARGIN, mmTopMargin));
                box.size = mm2px(Vec(DISPLAY_MM_WIDTH, DISPLAY_MM_HEIGHT));
                addButton(new TricorderButton_ToggleAxes(*this));
                addButton(new TricorderButton_ToggleNumbers(*this));
                addButton(new TricorderButton_SpinRight(*this));
                addButton(new TricorderButton_SpinLeft(*this));
                addButton(new TricorderButton_SpinUp(*this));
                addButton(new TricorderButton_SpinDown(*this));
                addButton(new TricorderButton_Home(*this));
                addButton(new TricorderButton_ZoomIn(*this));
                addButton(new TricorderButton_ZoomOut(*this));
                addButton(new TricorderButton_ClearPath(*this));
            }

            template <typename button_t>
            button_t* addButton(button_t* button)
            {
                addChild(button);
                buttonList.push_back(button);
                return button;
            }

            bool areButtonsVisible() const
            {
                return buttonFader.areButtonsVisible();
            }

            float buttonFade() const
            {
                return buttonFader.fade();
            }

            void draw(const DrawArgs& args) override
            {
                math::Rect r = box.zeroPos();
                nvgBeginPath(args.vg);
                nvgRect(args.vg, RECT_ARGS(r));
                nvgFillColor(args.vg, SCHEME_BLACK);
                nvgFill(args.vg);
                OpaqueWidget::draw(args);
            }

            void drawLayer(const DrawArgs& args, int layer) override
            {
                if (layer != 1)
                    return;

                const int n = ActivePointCount(module);
                if (n == 0)
                    return;

                renderList.clear();

                if (AxesAreVisible(*this))
                {
                    const float r = 4.0f;
                    Point origin(0, 0, 0);
                    addSegment(SegmentKind::Axis, -1, origin, Point(r, 0, 0));
                    addSegment(SegmentKind::Axis, -1, origin, Point(0, r, 0));
                    addSegment(SegmentKind::Axis, -1, origin, Point(0, 0, r));
                    drawLetterX(r);
                    drawLetterY(r);
                    drawLetterZ(r);
                }

                if (n < TRAIL_LENGTH)
                {
                    // The pointList has not yet reached full capacity.
                    // Render from the front to the back.
                    for (int i = 1; i < n; ++i)
                    {
                        const Point& p1 = module->pointList[i-1];
                        const Point& p2 = module->pointList[i];
                        addSegment(SegmentKind::Curve, i, p1, p2);
                    }
                    addTip(module->pointList[n-1]);
                }
                else
                {
                    // The pointList is full, so we treat it as a circular buffer.
                    int curr = module->nextPointIndex;      // the oldest point in the list
                    for (int i = 1; i < TRAIL_LENGTH; ++i)
                    {
                        int next = (curr + 1) % TRAIL_LENGTH;
                        const Point& p1 = module->pointList[curr];
                        const Point& p2 = module->pointList[next];
                        addSegment(SegmentKind::Curve, i, p1, p2);
                        curr = next;
                    }
                    addTip(module->pointList[curr]);
                }

                nvgSave(args.vg);
                Rect b = box.zeroPos();
                nvgScissor(args.vg, RECT_ARGS(b));
                render(args.vg, n);
                if (NumbersAreVisible(*this))
                    displayVoltageNumbers(args.vg);
                nvgResetScissor(args.vg);
                nvgRestore(args.vg);

                OpaqueWidget::drawLayer(args, layer);
            }

            void render(NVGcontext *vg, int pointCount)
            {
                // Sort in ascending order of line segment midpoint.
                std::sort(renderList.begin(), renderList.end());

                NVGcolor shadowColor = SCHEME_BLACK;
                const float shadowThickness = 0.7f;

                // Render in z-order to create correct blocking of segment visibility.
                for (const LineSegment& seg : renderList)
                {
                    if (seg.kind == SegmentKind::Tip)
                    {
                        nvgBeginPath(vg);
                        nvgStrokeColor(vg, SCHEME_WHITE);
                        nvgFillColor(vg, SCHEME_WHITE);
                        nvgCircle(vg, seg.vec1.x, seg.vec1.y, 1.5);
                        nvgFill(vg);
                    }
                    else
                    {
                        NVGcolor color = segmentColor(seg, pointCount);
                        float width = (seg.kind == SegmentKind::Axis) ? 1.5 : 1.8f;

                        // Shadow line
                        nvgBeginPath(vg);
                        nvgLineCap(vg, NVG_BUTT);
                        nvgStrokeColor(vg, shadowColor);
                        nvgStrokeWidth(vg, width + shadowThickness);
                        nvgMoveTo(vg, seg.vec1.x, seg.vec1.y);
                        nvgLineTo(vg, seg.vec2.x, seg.vec2.y);
                        nvgStroke(vg);

                        // Illuminated line
                        nvgBeginPath(vg);
                        nvgLineCap(vg, NVG_ROUND);
                        nvgStrokeColor(vg, color);
                        nvgStrokeWidth(vg, width);
                        nvgMoveTo(vg, seg.vec1.x, seg.vec1.y);
                        nvgLineTo(vg, seg.vec2.x, seg.vec2.y);
                        nvgStroke(vg);
                    }
                }
            }

            void formatVoltage(char *buffer, std::size_t size, float value, char varname)
            {
                if (size == 0)
                    return;

                if (!std::isfinite(value) || std::abs(value) > 999.0f)
                    snprintf(buffer, size, "%c = ***.***", varname);
                else
                    snprintf(buffer, size, "%c = %7.3f", varname, value);

                if (value > 0.0f)
                {
                    // Replace the rightmost space with a plus sign.
                    int space = -1;
                    for (int i = 0; buffer[i]; ++i)
                        if (buffer[i] == ' ')
                            space = i;

                    if (space >= 0)
                        buffer[space] = '+';
                }
            }

            void drawVoltageDirectionArrow(NVGcontext *vg, float x, float y, float delta)
            {
                if (std::abs(delta) < 1.0e-6f)
                    return;     // don't draw an arrow

                const float mmHorPos = 80.0f;
                const float mmHeight = 8.0f;
                const float mmArrowDx = 2.0f;
                const float yArrowFrac = 0.5f;
                float y1, dy;

                if (delta < 0.0f)
                {
                    // Draw a downward-pointing arrow.
                    y1 = y;
                    dy = -mmHeight;
                }
                else
                {
                    // Draw an upward-pointing arrow.
                    y1 = y - mmHeight;
                    dy = +mmHeight;
                }

                // (Bx, By) = tip of arrow
                float Bx = x + mmHorPos;
                float By = y1;

                // (Cx, Cy) = left arrow fin
                float Cx = Bx - mmArrowDx;
                float Cy = y1 + (1-yArrowFrac)*dy;

                // (Dx, Dy) = right arrow fin
                float Dx = Bx + mmArrowDx;
                float Dy = Cy;

                nvgBeginPath(vg);
                nvgLineCap(vg, NVG_ROUND);
                nvgStrokeColor(vg, SCHEME_WHITE);
                nvgStrokeWidth(vg, 0.7f);

                nvgMoveTo(vg, Cx, Cy);
                nvgLineTo(vg, Bx, By);
                nvgLineTo(vg, Dx, Dy);
                nvgClosePath(vg);

                nvgStroke(vg);
            }

            void displayVoltageNumbers(NVGcontext *vg)
            {
                if (module == nullptr)
                    return;

                std::shared_ptr<Font> font = APP->window->loadFont(fontPath);
                if (font)
                {
                    nvgFontSize(vg, 14);
                    nvgFontFaceId(vg, font->handle);
                    nvgFillColor(vg, nvgRGBA(0x8f, 0xff, 0x70, 0xff));
                    char buffer[30];
                    formatVoltage(buffer, sizeof(buffer), module->xcurr, 'X');
                    nvgText(vg, mm2px(NUMERIC_X_LEFT), mm2px(NUMERIC_TOP), buffer, nullptr);
                    formatVoltage(buffer, sizeof(buffer), module->ycurr, 'Y');
                    nvgText(vg, mm2px(NUMERIC_Y_LEFT), mm2px(NUMERIC_TOP), buffer, nullptr);
                    formatVoltage(buffer, sizeof(buffer), module->zcurr, 'Z');
                    nvgText(vg, mm2px(NUMERIC_Z_LEFT), mm2px(NUMERIC_TOP), buffer, nullptr);
                    drawVoltageDirectionArrow(vg, mm2px(NUMERIC_X_LEFT), mm2px(NUMERIC_TOP), module->xcurr - xprev);
                    drawVoltageDirectionArrow(vg, mm2px(NUMERIC_Y_LEFT), mm2px(NUMERIC_TOP), module->ycurr - yprev);
                    drawVoltageDirectionArrow(vg, mm2px(NUMERIC_Z_LEFT), mm2px(NUMERIC_TOP), module->zcurr - zprev);
                    xprev = module->xcurr;
                    yprev = module->ycurr;
                    zprev = module->zcurr;
                }
            }

            NVGcolor segmentColor(const LineSegment& seg, int pointCount) const
            {
                NVGcolor nearColor;
                NVGcolor farColor;
                switch (seg.kind)
                {
                case SegmentKind::Curve:
                    nearColor = SCHEME_CYAN;
                    farColor = SCHEME_PURPLE;
                    break;

                case SegmentKind::Axis:
                    nearColor = nvgRGB(0xf0, 0xf0, 0xa0);
                    farColor = nvgRGB(0x30, 0x30, 0x20);
                    break;

                default:
                    nearColor = SCHEME_RED;
                    farColor = SCHEME_RED;
                    break;
                }

                float denom = static_cast<float>(std::max(40, pointCount));
                float factor = static_cast<float>(std::min(20, std::max(5, pointCount)));
                float opacity = (seg.index < 0) ? 1.0f : std::min(1.0f, (factor * seg.index) / denom);

                float prox = std::max(0.0f, std::min(1.0f, seg.prox));
                float dist = 1 - prox;
                NVGcolor color;
                color.a = 1;
                color.r = opacity*(prox*nearColor.r + dist*farColor.r);
                color.g = opacity*(prox*nearColor.g + dist*farColor.g);
                color.b = opacity*(prox*nearColor.b + dist*farColor.b);
                return color;
            }

            void addTip(const Point& point)
            {
                float prox;
                Vec vec = project(point, prox);
                // Give the tip a slight proximity bonus to account for its small radius.
                // We want to draw the tip after the line segment it is connected to.
                prox += 0.1;
                renderList.push_back(LineSegment::MakeTip(vec, prox));
            }

            void addSegment(SegmentKind kind, int index, const Point& point1, const Point& point2)
            {
                float prox1;
                Vec vec1 = project(point1, prox1);
                float prox2;
                Vec vec2 = project(point2, prox2);
                expandSegment(0, kind, index, vec1, vec2, prox1, prox2, point1, point2);
            }

            void expandSegment(
                int depth,
                SegmentKind kind,
                int index,
                const Vec& vec1,
                const Vec& vec2,
                float prox1,
                float prox2,
                const Point& point1,
                const Point& point2)
            {
                // If the endpoints are close enough to the same z-level (observer proximity)
                // or we have hit recursion depth limit, add a single line segment.
                if (depth == 5 || std::abs(prox1 - prox2) < 0.05f)
                {
                    renderList.push_back(LineSegment(vec1, vec2, (prox1+prox2)/2, kind, index));
                }
                else
                {
                    // Recursively split the line segment in two to handle inclination toward observer.
                    Point pointm((point1.x + point2.x)/2, (point1.y + point2.y)/2, (point1.z + point2.z)/2);
                    float proxm;
                    Vec vecm = project(pointm, proxm);
                    expandSegment(1+depth, kind, index, vec1, vecm, prox1, proxm, point1, pointm);
                    expandSegment(1+depth, kind, index, vecm, vec2, proxm, prox2, pointm, point2);
                }
            }

            void drawLetterX(float r)
            {
                const float La = r * 1.05f;
                const float Lb = r * 1.15f;
                const float Lc = r * 0.03f;
                // We used to draw the letter X using two line segments.
                // The problem is that both line segments have the same midpoint,
                // which causes the z-order sort to keep flipping back and forth
                // between which segment was drawn on top. This caused an unpleasant
                // "vibration". Fix this by breaking one of the segments into two parts.
                Point mid((La+Lb)/2, 0, 0);
                addSegment(SegmentKind::Axis, -1, Point(La, 0, -Lc), mid);
                addSegment(SegmentKind::Axis, -1, mid, Point(Lb, 0, +Lc));
                addSegment(SegmentKind::Axis, -1, Point(La, 0, +Lc), Point(Lb, 0, -Lc));
            }

            void drawLetterY(float r)
            {
                const float La = r * 1.05f;
                const float Lb = r * 0.03f;
                const float Lc = r * 1.10f;
                const float Ld = r * 1.15f;
                addSegment(SegmentKind::Axis, -1, Point(0, Lc, 0), Point(  0, La, 0));
                addSegment(SegmentKind::Axis, -1, Point(0, Lc, 0), Point(-Lb, Ld, 0));
                addSegment(SegmentKind::Axis, -1, Point(0, Lc, 0), Point(+Lb, Ld, 0));
            }

            void drawLetterZ(float r)
            {
                const float La = r * 1.05f;
                const float Lb = r * 1.15f;
                const float Lc = r * 0.03f;
                addSegment(SegmentKind::Axis, -1, Point(-Lc, 0, La), Point(+Lc, 0, La));
                addSegment(SegmentKind::Axis, -1, Point(+Lc, 0, La), Point(-Lc, 0, Lb));
                addSegment(SegmentKind::Axis, -1, Point(-Lc, 0, Lb), Point(+Lc, 0, Lb));
            }

            Vec project(const Point& p, float& prox) const
            {
                if (module == nullptr)
                {
                    prox = 0;
                    return Vec(0, 0);
                }

                // Apply the rotation matrix to the 3D point.
                Point q = module->orientation.rotate(p);

                // Project the 3D point 'p' onto a screen location Vec.
                float s = module->voltageScale;
                float sx = (DISPLAY_SCALE/2) * (1 + q.x/s);
                float sy = (DISPLAY_SCALE/2) * (1 - q.y/s);
                prox = (1 + q.z/s) / 2;
                return mm2px(Vec(sx, sy));
            }

            void step() override
            {
                if (module == nullptr || module->bypassing)
                    return;

                module->stepOrientation();
                buttonFader.step();
            }

            void onHover(const HoverEvent& e) override
            {
                if (ownsMouse)
                {
                    if (hoverMousePos.equals(e.pos))
                    {
                        // The mouse is not moving.
                        buttonFader.onMouseStill();
                    }
                    else
                    {
                        // The mouse is moving.
                        hoverMousePos = e.pos;
                        buttonFader.onMouseMove();
                    }
                }
                OpaqueWidget::onHover(e);
            }

            void onButton(const ButtonEvent& e) override
            {
                OpaqueWidget::onButton(e);
                if (e.button != GLFW_MOUSE_BUTTON_LEFT)
                {
                    // Prevent right-click from launching context menu inside the display area.
                    // Showing the context menu causes buttons to disappear for no reason.
                    // If people want the context menu, they have to click outside the display area.
                    if (!e.isConsumed())
                        e.consume(this);
                }
            }

            void onEnter(const EnterEvent& e) override
            {
                ownsMouse = true;
                buttonFader.onEnterDisplayArea();
            }

            void onLeave(const LeaveEvent& e) override
            {
                ownsMouse = false;
                buttonFader.onLeaveDisplayArea();
            }

            void onEnterButton()
            {
                buttonFader.onEnterButton();
            }

            void onLeaveButton()
            {
                buttonFader.onLeaveButton();
            }

            void onDragStart(const DragStartEvent& e) override
            {
                if (e.button != GLFW_MOUSE_BUTTON_LEFT)
                    return;

                if (module != nullptr)
                {
                    // Stop auto-rotation if in effect.
                    module->selectRotationMode(0, 0);
                }

                isDragging = true;
            }

            void onDragEnd(const DragEndEvent& e) override
            {
                if (e.button != GLFW_MOUSE_BUTTON_LEFT)
                    return;

                isDragging = false;
            }

            void onDragMove(const DragMoveEvent& e) override
            {
                if (e.button != GLFW_MOUSE_BUTTON_LEFT)
                    return;

                if (module != nullptr)
                {
                    // Adjust latitude/longitude angles based on mouse movement.
                    const float scale = (0.35 * module->zoomFactor()) / DISPLAY_SCALE;
                    const float lon = scale * e.mouseDelta.x;
                    const float lat = scale * e.mouseDelta.y;
                    module->updateOrientation(lat, lon);
                    e.consume(this);
                }
            }
        };


        bool AreButtonsVisible(const TricorderDisplay& display)
        {
            // Buttons are either all visible or all invisible.
            // They are all visible if the mouse is inside the display or
            // inside any of the buttons. Otherwise all buttons are invisible.
            // This is a distinction because the buttons are OpaqueWidget,
            // meaning once the mouse enters a button, it "leaves" the display.

            // Buttons are never shown when the module is not active,
            // whether because of being bypassed or disconnected from an input signal.
            if (0 == ActivePointCount(display.module))
                return false;

            // Hide the buttons while dragging orientation.
            if (display.isDragging)
                return false;

            return display.areButtonsVisible();
        }


        float ButtonFade(const TricorderDisplay& display)
        {
            return display.buttonFade();
        }


        void OnEnterDisplayButton(TricorderDisplay& display)
        {
            display.onEnterButton();
        }


        void OnLeaveDisplayButton(TricorderDisplay& display)
        {
            display.onLeaveButton();
        }


        void SelectRotationMode(const TricorderDisplay& display, int longitudeDirection, int latitudeDirection)
        {
            if (display.module != nullptr)
                display.module->selectRotationMode(longitudeDirection, latitudeDirection);
        }


        void ResetPerspective(const TricorderDisplay& display)
        {
            if (display.module != nullptr)
                display.module->resetPerspective();
        }


        void ToggleAxisVisibility(const TricorderDisplay& display)
        {
            if (display.module != nullptr)
                display.module->axesAreVisible = !display.module->axesAreVisible;
        }


        void ToggleNumbersVisibility(const TricorderDisplay& display)
        {
            if (display.module != nullptr)
                display.module->numbersAreVisible = !display.module->numbersAreVisible;
        }


        bool AxesAreVisible(const TricorderDisplay& display)
        {
            return (display.module != nullptr) && display.module->axesAreVisible;
        }


        bool NumbersAreVisible(const TricorderDisplay& display)
        {
            return (display.module != nullptr) && display.module->numbersAreVisible;
        }


        void AdjustZoom(const TricorderDisplay& display, int adjust)
        {
            if (display.module != nullptr)
                display.module->adjustZoom(adjust);
        }


        void ClearPath(TricorderDisplay& display)
        {
            if (display.module != nullptr)
                display.module->resetPointList();
        }


        struct RotationSpeedSlider : ui::Slider
        {
            explicit RotationSpeedSlider(SapphireQuantity *_quantity)
            {
                quantity = _quantity;
                box.size.x = 200.0f;
            }
        };


        struct TricorderWidget : SapphireReloadableModuleWidget
        {
            TricorderModule* tricorderModule{};

            explicit TricorderWidget(TricorderModule *module)
                : SapphireReloadableModuleWidget(asset::plugin(pluginInstance, "res/tricorder.svg"))
                , tricorderModule(module)
            {
                setModule(module);

                // Load the SVG and place all controls at their correct coordinates.
                reloadPanel();

                addChild(new TricorderDisplay(module));
            }

            void appendContextMenu(Menu* menu) override
            {
                if (tricorderModule != nullptr)
                {
                    menu->addChild(new MenuSeparator);
                    menu->addChild(new RotationSpeedSlider(tricorderModule->rotationSpeedQuantity));
                }
            }
        };
    }
}

Model *modelTricorder = createSapphireModel<Sapphire::Tricorder::TricorderModule, Sapphire::Tricorder::TricorderWidget>(
    "Tricorder",
    Sapphire::VectorRole::SenderAndReceiver
);
