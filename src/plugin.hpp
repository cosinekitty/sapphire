#pragma once
#include <rack.hpp>
#include <vector>

// Sapphire for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

using namespace rack;

// Declare the Plugin, defined in plugin.cpp
extern Plugin* pluginInstance;

// Declare each Model, defined in each module source file
extern Model* modelBash;
extern Model* modelElastika;
extern Model* modelFrolic;
extern Model* modelGlee;
extern Model* modelHiss;
extern Model* modelMoots;
extern Model* modelNucleus;
extern Model* modelPolynucleus;
extern Model* modelSpatula;
extern Model* modelTin;
extern Model* modelTout;
extern Model* modelTricorder;
extern Model* modelTubeUnit;

namespace Sapphire
{
    enum class VectorRole
    {
        None,
        Sender,
        Receiver,
        SenderAndReceiver,
    };

    struct ModelInfo
    {
        static ModelInfo *front;
        ModelInfo *next = nullptr;
        rack::plugin::Model* model;
        VectorRole vectorRole;

        ModelInfo(rack::plugin::Model* _model, VectorRole _vectorRole)
            : model(_model)
            , vectorRole(_vectorRole)
        {
        }

        static void insert(Model *_model, VectorRole _vectorRole)
        {
            ModelInfo *info = new ModelInfo(_model, _vectorRole);
            info->next = front;
            front = info;
        }

        static ModelInfo* search(const Model *model)
        {
            if (model != nullptr)
                for (ModelInfo *info = front; info != nullptr; info = info->next)
                    if (info->model == model)
                        return info;

            return nullptr;
        }

        static bool canReceiveVectors(const Model* model)
        {
            ModelInfo* info = search(model);
            if (info == nullptr)
                return false;

            return (info->vectorRole == VectorRole::Receiver) || (info->vectorRole == VectorRole::SenderAndReceiver);
        }

        static bool canSendVectors(const Model* model)
        {
            ModelInfo* info = search(model);
            if (info == nullptr)
                return false;

            return (info->vectorRole == VectorRole::Sender) || (info->vectorRole == VectorRole::SenderAndReceiver);
        }
    };

    namespace Tricorder
    {
        struct MessageHeader
        {
            std::size_t size;
            char signature[4];
            std::uint32_t version;

            MessageHeader(std::uint32_t _version, std::size_t _size)
                : size(_size)
                , version(_version)
            {
                std::memcpy(signature, "Tcdr", 4);
            }
        };

        struct Message
        {
            const MessageHeader header;
            float x{};
            float y{};
            float z{};
            char flag{};

            // Add any future fields here. Do not change anything above this line.
            // Update the version number as needed in the constructor call below.

            Message()
                : header(2, sizeof(Message))
                {}

            void setVector(float _x, float _y, float _z, bool reset)
            {
                x = _x;
                y = _y;
                z = _z;
                flag = reset ? 'V' : 'v';
            }

            bool isResetRequested() const
            {
                return flag == 'V';
            }
        };

        inline bool IsValidMessage(const Message *message)
        {
            return
                (message != nullptr) &&
                (message->header.size >= sizeof(Message)) &&
                (0 == memcmp(message->header.signature, "Tcdr", 4)) &&
                (message->header.version >= 2);
        }

        inline bool IsVectorMessage(const Message *message)
        {
            return IsValidMessage(message) && (message->flag == 'v' || message->flag == 'V');
        }

        struct VectorSender     // allows another module to send vectors into Tricorder/Tout from the left side
        {
            Message buffer[2];
            Module& parentModule;

            static bool isVectorReceiver(const Module* module)
            {
                return (module != nullptr) && ModelInfo::canReceiveVectors(module->model);
            }

            explicit VectorSender(Module& module)
                : parentModule(module)
            {
                module.rightExpander.producerMessage = &buffer[0];
                module.rightExpander.consumerMessage = &buffer[1];
            }

            void sendVector(float x, float y, float z, bool reset)
            {
                Tricorder::Message& prod = *static_cast<Tricorder::Message*>(parentModule.rightExpander.producerMessage);
                prod.setVector(x, y, z, reset);
                parentModule.rightExpander.requestMessageFlip();
            }

            bool isVectorReceiverConnectedOnRight() const
            {
                return isVectorReceiver(parentModule.rightExpander.module);
            }
        };


        struct VectorReceiver   // shared code for Tricorder and Tout to receive vectors from a module on the left
        {
            Message buffer[2];
            Module& parentModule;

            static bool isVectorSender(const Module* module)
            {
                return (module != nullptr) && ModelInfo::canSendVectors(module->model);
            }

            explicit VectorReceiver(Module& module)
                : parentModule(module)
            {
                module.leftExpander.producerMessage = &buffer[0];
                module.leftExpander.consumerMessage = &buffer[1];
            }

            bool isVectorSenderConnectedOnLeft() const
            {
                return isVectorSender(parentModule.leftExpander.module);
            }

            Message* inboundVectorMessage() const
            {
                const Module* lm = parentModule.leftExpander.module;
                if (isVectorSender(lm))
                {
                    Message* message = static_cast<Message *>(lm->rightExpander.consumerMessage);
                    if (IsVectorMessage(message))
                        return message;
                }
                return nullptr;
            }
        };
    }

    struct ControlGroup     // represents the combination: knob + CV input + attenuverter
    {
        std::string name;
        int yGrid;
        int xGrid;
        int paramId;
        int attenId;
        int inputId;
        float minValue;
        float maxValue;
        float defaultValue;
        std::string unit;
        float displayBase;
        float displayMultiplier;

        ControlGroup(
            const std::string& _name,
            int _yGrid,
            int _xGrid,
            int _paramId,
            int _attenId,
            int _inputId,
            float _minValue,
            float _maxValue,
            float _defaultValue,
            const std::string& _unit = "",
            float _displayBase = 0.0f,
            float _displayMultiplier = 1.0f
        )
            : name(_name)
            , yGrid(_yGrid)
            , xGrid(_xGrid)
            , paramId(_paramId)
            , attenId(_attenId)
            , inputId(_inputId)
            , minValue(_minValue)
            , maxValue(_maxValue)
            , defaultValue(_defaultValue)
            , unit(_unit)
            , displayBase(_displayBase)
            , displayMultiplier(_displayMultiplier)
        {}
    };

    // Custom controls for Sapphire modules.

    struct SapphirePort : app::SvgPort
    {
        SapphirePort()
        {
            setSvg(Svg::load(asset::plugin(pluginInstance, "res/port.svg")));
        }
    };


    struct SapphireQuantity : ParamQuantity
    {
        float value = 0.0f;
        bool changed = true;

        SapphireQuantity()
        {
            randomizeEnabled = false;
        }

        void setValue(float newValue) override
        {
            float clamped = rack::math::clamp(newValue, getMinValue(), getMaxValue());
            if (clamped != value)
            {
                changed = true;
                value = clamped;
            }
        }

        float getValue() override { return value; }

        void setDisplayValue(float displayValue) override { setValue(displayValue); }

        void save(json_t* root, const char *name)
        {
            json_object_set_new(root, name, json_real(value));
        }

        void load(json_t* root, const char *name)
        {
            json_t *level = json_object_get(root, name);
            if (json_is_number(level))
            {
                double x = json_number_value(level);
                setValue(x);
            }
        }

        void initialize()
        {
            float x = getDefaultValue();
            setValue(x);
        }
    };


    const float DC_REJECT_MIN_FREQ = 20.0f;
    const float DC_REJECT_MAX_FREQ = 400.0f;
    const float DC_REJECT_DEFAULT_FREQ = 20.0f;


    struct DcRejectQuantity : SapphireQuantity
    {
        std::string getDisplayValueString() override
        {
            return string::f("%i", (int)(math::normalizeZero(value) + 0.5f));
        }
    };


    struct DcRejectSlider : ui::Slider
    {
        explicit DcRejectSlider(DcRejectQuantity *_quantity)
        {
            quantity = _quantity;
            box.size.x = 200.0f;        // without this, the menu display gets messed up
        }
    };


    const float AGC_LEVEL_MIN = 1.0f;
    const float AGC_LEVEL_DEFAULT = 4.0f;
    const float AGC_LEVEL_MAX = 10.0f;
    const float AGC_DISABLE_MIN = 10.1f;
    const float AGC_DISABLE_MAX = 10.2f;


    struct AgcLevelQuantity : SapphireQuantity
    {
        bool isAgcEnabled() const { return value < AGC_DISABLE_MIN; }
        float clampedAgc() const { return clamp(value, AGC_LEVEL_MIN, AGC_LEVEL_MAX); }

        std::string getDisplayValueString() override
        {
            return isAgcEnabled() ? string::f("%0.2f V", clampedAgc()) : "OFF";
        }
    };


    struct AgcLevelSlider : ui::Slider
    {
        explicit AgcLevelSlider(AgcLevelQuantity *_quantity)
        {
            quantity = _quantity;
            box.size.x = 200.0f;
        }
    };

    // My custom overlay class is very similar to SvgPanel,
    // only without a border drawn around it.
    // It provides a transparent overlay on top of my main SvgPanel,
    // and enables hiding/showing layers at will.
    struct SvgOverlay : Widget
    {
        FramebufferWidget* fb;
        SvgWidget *sw;
        std::shared_ptr<window::Svg> svg;

        explicit SvgOverlay(std::shared_ptr<window::Svg> _svg)
        {
            fb = new FramebufferWidget;
            addChild(fb);

            sw = new SvgWidget;
            fb->addChild(sw);

            svg = _svg;
            sw->setSvg(_svg);

            box.size = fb->box.size = sw->box.size;
        }

        static SvgOverlay* Load(std::string relativeFileName)
        {
            std::string filename = asset::plugin(pluginInstance, relativeFileName);
            std::shared_ptr<Svg> svg = Svg::load(filename);
            return new SvgOverlay(svg);
        }

        void step() override
        {
            fb->oversample = (APP->window->pixelRatio < 2.0f) ? 2.0f : 1.0f;
            Widget::step();
        }
    };


    class GateTriggerReceiver
    {
    private:
        float prevVoltage{};
        bool gate{};
        bool trigger{};

    public:
        bool isGateActive() const { return gate; }
        bool isTriggerActive() const { return trigger; }

        void initialize()
        {
            prevVoltage = 0;
            gate = false;
            trigger = false;
        }

        void update(float voltage)
        {
            trigger = false;
            if (prevVoltage < 1.0f && voltage >= 1.0f)
            {
                trigger = !gate;
                gate = true;
            }
            else if (prevVoltage >= 0.1f && voltage < 0.1f)
            {
                gate = false;
            }
            prevVoltage = voltage;
        }

        bool updateGate(float voltage)
        {
            update(voltage);
            return gate;
        }

        bool updateTrigger(float voltage)
        {
            update(voltage);
            return trigger;
        }
    };


    class TriggerSender
    {
    private:
        const float duration = 0.001f;  // a trigger should last at least one millisecond
        float elapsed = 0;
        bool isFiring = false;

    public:
        void initialize()
        {
            elapsed = 0;
            isFiring = false;
        }

        float process(float dt, bool fire)
        {
            if (fire)
            {
                elapsed = 0;
                isFiring = true;
            }
            if (isFiring)
            {
                if (elapsed >= duration)
                    isFiring = false;
                elapsed += dt;
                return 10;
            }
            return 0;
        }
    };


    const float AttenuverterLowSensitivityDenom = 10;

    struct SapphireParamInfo
    {
        bool isLowSensitive{};

        SapphireParamInfo()
        {
            initialize();
        }

        void initialize()
        {
            isLowSensitive = false;
        }
    };

    struct SapphireModule : public Module
    {
        Tricorder::VectorSender vectorSender;
        Tricorder::VectorReceiver vectorReceiver;
        std::vector<SapphireParamInfo> paramInfo;

        explicit SapphireModule(std::size_t nparams)
            : vectorSender(*this)
            , vectorReceiver(*this)
            , paramInfo(nparams)
            {}

        float getControlValue(int paramId, int attenId, int inputId, float minValue = 0, float maxValue = 1)
        {
            float slider = params[paramId].getValue();
            float cv = inputs[inputId].getVoltageSum();
            // When the attenuverter is set to 100%, and the cv is +5V, we want
            // to swing a slider that is all the way down (minSlider)
            // to act like it is all the way up (maxSlider).
            // Thus we allow the complete range of control for any CV whose
            // range is [-5, +5] volts.
            float attenu = params[attenId].getValue();
            if (isLowSensitive(attenId))
                attenu /= AttenuverterLowSensitivityDenom;
            slider += attenu*(cv / 5)*(maxValue - minValue);
            return clamp(slider, minValue, maxValue);
        }

        float getChaosValue(int paramId, int chaosId, float cv, float minValue = 0, float maxValue = 1)
        {
            float slider = params[paramId].getValue();
            // When the attenuverter is set to 100%, and the cv is +5V, we want
            // to swing a slider that is all the way down (minSlider)
            // to act like it is all the way up (maxSlider).
            // Thus we allow the complete range of control for any CV whose
            // range is [-5, +5] volts.
            float attenu = params[chaosId].getValue();
            if (isLowSensitive(chaosId))
                attenu /= AttenuverterLowSensitivityDenom;
            slider += attenu*(cv / 5)*(maxValue - minValue);
            return clamp(slider, minValue, maxValue);
        }

        bool isLowSensitive(int attenId) const
        {
            return paramInfo.at(attenId).isLowSensitive;
        }

        bool *lowSensitiveFlag(int attenId)
        {
            return &paramInfo.at(attenId).isLowSensitive;
        }

        bool isVectorReceiverConnectedOnRight() const
        {
            return vectorSender.isVectorReceiverConnectedOnRight();
        }

        bool isVectorSenderConnectedOnLeft() const
        {
            return vectorReceiver.isVectorSenderConnectedOnLeft();
        }

        json_t* dataToJson() override
        {
            json_t* root = json_object();

            // We have to save/restore the attenuverter sensitivity settings.
            // Represent the settings by saving a list of all the integer attenuverter
            // IDs that have low sensitivity enabled.
            const int nparams = static_cast<int>(paramInfo.size());
            json_t* list = json_array();
            for (int attenId = 0; attenId < nparams; ++attenId)
                if (isLowSensitive(attenId))
                    json_array_append(list, json_integer(attenId));

            json_object_set_new(root, "lowSensitivityAttenuverters", list);

            return root;
        }

        void dataFromJson(json_t* root) override
        {
            // Restore attenuverter low-sensitivity settings.
            // If the attenuverter ID is in the list, low-sensitivity is enabled.
            // If the attenuverter ID is absent from the list, low-sensitivity is disabled.
            // Therefore, we need to set the flag true/false in either case.
            // Strategy: re-initialize paramInfo, then if possible, come back
            // and set the low sensitivity flags to true for the knobs that need it.
            // This way, we at least clear out the state even if something is wrong the the JSON.
            const int nparams = static_cast<int>(paramInfo.size());
            for (int attenId = 0; attenId < nparams; ++attenId)
                paramInfo.at(attenId).initialize();

            json_t* list = json_object_get(root, "lowSensitivityAttenuverters");
            if (list != nullptr)
            {
                std::size_t listLength = static_cast<int>(json_array_size(list));
                for (std::size_t listIndex = 0; listIndex < listLength; ++listIndex)
                {
                    json_t *item = json_array_get(list, listIndex);
                    if (json_is_integer(item))
                    {
                        int attenId = static_cast<int>(json_integer_value(item));
                        if (attenId >= 0 && attenId < nparams)
                            paramInfo.at(attenId).isLowSensitive = true;
                    }
                }
            }
        }
    };


    struct SapphireAutomaticLimiterModule : public SapphireModule   // a Sapphire module with a warning light on the OUTPUT knob
    {
        bool enableLimiterWarning = true;
        int recoveryCountdown = 0;      // positive integer when we make OUTPUT knob pink to indicate "NAN crash"

        explicit SapphireAutomaticLimiterModule(std::size_t nparams)
            : SapphireModule(nparams)
            {}

        virtual double getAgcDistortion() const = 0;

        virtual NVGcolor getWarningColor()
        {
            if (recoveryCountdown > 0)
            {
                // The module is recovering from non-finite (NAN/infinite) output.
                // Inflict an obnoxiously bright pink OUTPUT knob glow on the user!
                return nvgRGBA(0xff, 0x00, 0xff, 0xb0);
            }

            const double distortion = getAgcDistortion();
            if (!enableLimiterWarning || distortion <= 0.0)
                return nvgRGBA(0, 0, 0, 0);     // no warning light

            double decibels = 20.0 * std::log10(1.0 + distortion);
            double scale = clamp(decibels / 24.0);

            int red   = colorComponent(scale, 0x90, 0xff);
            int green = colorComponent(scale, 0x20, 0x50);
            int blue  = 0x00;
            int alpha = 0x70;

            return nvgRGBA(red, green, blue, alpha);
        }

        static int colorComponent(double scale, int lo, int hi)
        {
            return clamp(static_cast<int>(round(lo + scale*(hi-lo))), lo, hi);
        }
    };


    class WarningLightWidget : public LightWidget
    {
    private:
        SapphireAutomaticLimiterModule *alModule;

    public:
        explicit WarningLightWidget(SapphireAutomaticLimiterModule *_alModule)
            : alModule(_alModule)
        {
            borderColor = nvgRGBA(0x00, 0x00, 0x00, 0x00);      // don't draw a circular border
            bgColor     = nvgRGBA(0x00, 0x00, 0x00, 0x00);      // don't mess with the knob behind the light
        }

        void drawLayer(const DrawArgs& args, int layer) override
        {
            if (layer == 1)
                color = alModule ? alModule->getWarningColor() : nvgRGBA(0, 0, 0, 0);

            LightWidget::drawLayer(args, layer);
        }
    };


    class Stopwatch     // Similar to the System.Diagnostics.Stopwatch class in C#
    {
    private:
        bool running = false;
        double initialTime = 0;
        double totalSeconds = 0;

    public:
        void reset()
        {
            running = false;
            initialTime = 0;
            totalSeconds = 0;
        }

        void start()
        {
            if (!running)       // ignore redundant calls
            {
                running = true;
                initialTime = rack::system::getTime();
                totalSeconds = 0;
            }
        }

        double stop()
        {
            if (running)        // ignore redundant calls
            {
                running = false;
                totalSeconds += rack::system::getTime() - initialTime;
                initialTime = 0;
            }
            return totalSeconds;
        }

        double elapsedSeconds() const
        {
            double elapsed = totalSeconds;
            if (running)
                elapsed += rack::system::getTime() - initialTime;
            return elapsed;
        }

        double restart()
        {
            double elapsed = stop();
            start();
            return elapsed;
        }
    };
}


// Keep this in the global namespace, not inside "Sapphire".
template <class TModule, class TModuleWidget>
rack::plugin::Model* createSapphireModel(std::string slug, Sapphire::VectorRole vectorRole)
{
    Model *model = rack::createModel<TModule, TModuleWidget>(slug);
    Sapphire::ModelInfo::insert(model, vectorRole);
    return model;
}
