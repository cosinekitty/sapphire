// Sapphire for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire
#pragma once
#include "plugin.hpp"
namespace Sapphire
{
    inline int VcvSafeChannelCount(int count)
    {
        return std::clamp<int>(count, 0, PORT_MAX_CHANNELS);
    }

    enum class ExpanderRole
    {
        None            = 0,
        VectorSender    = 0x01,
        VectorReceiver  = 0x02,
        ChaosOpSender   = 0x04,     // Chaops
        ChaosOpReceiver = 0x08,     // Frolic, Glee, Lark
        MultiTap        = 0x10,     // Echo, EchoTap, EchoOut
    };

    inline constexpr ExpanderRole Both(ExpanderRole a, ExpanderRole b)
    {
        return static_cast<ExpanderRole>(static_cast<int>(a) | static_cast<int>(b));
    }

    const ExpanderRole VectorSenderAndReceiver = Both(
        Sapphire::ExpanderRole::VectorSender,
        Sapphire::ExpanderRole::VectorReceiver
    );

    const ExpanderRole ChaosModuleRoles = Both(
        Sapphire::ExpanderRole::VectorSender,
        Sapphire::ExpanderRole::ChaosOpReceiver
    );

    struct ModelInfo
    {
        static ModelInfo *front;
        ModelInfo *next = nullptr;
        rack::plugin::Model* model;
        ExpanderRole roles;

        ModelInfo(rack::plugin::Model* _model, ExpanderRole _roles)
            : model(_model)
            , roles(_roles)
        {
        }

        static void insert(Model *_model, ExpanderRole _roles)
        {
            ModelInfo *info = new ModelInfo(_model, _roles);
            info->next = front;
            front = info;
        }

        static ModelInfo* search(const Model *model)
        {
            if (model)
                for (ModelInfo *info = front; info; info = info->next)
                    if (info->model == model)
                        return info;

            return nullptr;
        }

        static bool hasRole(const Module* module, ExpanderRole role)
        {
            if (module == nullptr)
                return false;

            ModelInfo* info = search(module->model);
            if (info == nullptr)
                return false;

            return 0 != (static_cast<int>(info->roles) & static_cast<int>(role));
        }
    };

    template <typename enum_t>
    void jsonSetEnum(json_t* root, const char *key, enum_t value)
    {
        json_object_set_new(root, key, json_integer(static_cast<int>(value)));
    }

    template <typename enum_t>
    void jsonLoadEnum(json_t* root, const char *key, enum_t& value)
    {
        json_t* js = json_object_get(root, key);
        if (json_is_integer(js))
            value = static_cast<enum_t>(json_integer_value(js));
    }

    inline void jsonSetBool(json_t* root, const char *key, bool value)
    {
        json_object_set_new(root, key, json_boolean(value));
    }

    inline void jsonLoadBool(json_t* root, const char* key, bool& value)
    {
        json_t* js = json_object_get(root, key);
        if (json_is_boolean(js))
            value = json_boolean_value(js);
    }

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
                message &&
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
                return ModelInfo::hasRole(module, ExpanderRole::VectorReceiver);
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
                return ModelInfo::hasRole(module, ExpanderRole::VectorSender);
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

            const Message* inboundVectorMessage() const
            {
                const Module* lm = parentModule.leftExpander.module;
                if (isVectorSender(lm))
                {
                    auto message = static_cast<const Message *>(lm->rightExpander.consumerMessage);
                    if (IsVectorMessage(message))
                        return message;
                }
                return nullptr;
            }
        };
    }

    namespace ChaosOperators
    {
        const unsigned MemoryCount = 16;

        struct Message
        {
            bool store  = false;
            bool recall = false;
            bool freeze = false;
            unsigned memoryIndex = 0;       // 0..(MemoryCount-1)
            float morph = 0;
        };

        struct Sender
        {
            Message buffer[2];
            Module& parentModule;

            explicit Sender(Module& module)
                : parentModule(module)
            {
                module.rightExpander.producerMessage = &buffer[0];
                module.rightExpander.consumerMessage = &buffer[1];
            }

            void send(const Message& message)
            {
                Message& prod = *static_cast<Message*>(parentModule.rightExpander.producerMessage);
                prod = message;
                parentModule.rightExpander.requestMessageFlip();
            }

            bool isReceiverConnectedOnRight() const
            {
                return ModelInfo::hasRole(parentModule.rightExpander.module, ExpanderRole::ChaosOpReceiver);
            }
        };


        struct Receiver
        {
            Message buffer[2];
            Module& parentModule;

            explicit Receiver(Module& module)
                : parentModule(module)
            {
                module.leftExpander.producerMessage = &buffer[0];
                module.leftExpander.consumerMessage = &buffer[1];
            }

            const Message* inboundMessage() const
            {
                const Module* lm = parentModule.leftExpander.module;
                if (ModelInfo::hasRole(lm, ExpanderRole::ChaosOpSender))
                    return static_cast<const Message *>(lm->rightExpander.consumerMessage);
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
            float clamped = std::clamp(newValue, getMinValue(), getMaxValue());
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
            changed = true;     // force initial update of this quantity's consumer
        }

        bool isChangedOneShot()
        {
            const bool wasChanged = changed;
            changed = false;
            return wasChanged;
        }
    };


    struct ChannelCountQuantity : SapphireQuantity
    {
        int getDesiredChannelCount() const
        {
            int n = static_cast<int>(std::round(value));
            return std::clamp(n, 1, 16);
        }

        std::string getDisplayValueString() override
        {
            return string::f("%d", getDesiredChannelCount());
        }
    };


    struct ChannelCountSlider : ui::Slider
    {
        explicit ChannelCountSlider(ChannelCountQuantity *_quantity)
        {
            quantity = _quantity;
            box.size.x = 200;
        }

        void draw(const DrawArgs& args) override
        {
            // [Don Cross] Copied and modified from: Rack/src/ui/Slider.cpp

            BNDwidgetState state = BND_DEFAULT;
            if (APP->event->hoveredWidget == this)
                state = BND_HOVER;
            if (APP->event->draggedWidget == this)
                state = BND_ACTIVE;

            float progress = 0;

            // Render the slider snapped to the nearest integer number of channels.
            // Compensate for rescaling: progress ranges 0 to 1, but channels from 1 to 16.
            // There is also a 0.5 "buffer" in the channel count at the top and bottom of the range.
            auto ccq = static_cast<const ChannelCountQuantity*>(quantity);
            if (ccq)
                progress = std::clamp((ccq->getDesiredChannelCount() - 0.5f) / 16.0f, 0.0f, 1.0f);

            std::string text = quantity ? quantity->getString() : "";

            // If parent is a Menu, make corners sharp
            auto parentMenu = dynamic_cast<const ui::Menu*>(getParent());
            int flags = parentMenu ? BND_CORNER_ALL : BND_CORNER_NONE;
            bndSlider(args.vg, 0.0, 0.0, box.size.x, box.size.y, flags, state, progress, text.c_str(), NULL);
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


    struct AgcLevelQuantity : SapphireQuantity
    {
        float levelMin{};
        float levelMax{};
        float disableMin{};

        bool isAgcEnabled() const { return value < disableMin; }
        float clampedAgc() const { return std::clamp(value, levelMin, levelMax); }

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


    constexpr float FlashDurationSeconds = 0.05;


    class AnimatedTriggerReceiver
    {
    private:
        GateTriggerReceiver tr;
        float flashSecondsRemaining = 0;

    public:
        void initialize()
        {
            tr.initialize();
            flashSecondsRemaining = 0;
        }

        bool updateTrigger(float voltage, float sampleRateHz)
        {
            bool trigger = tr.updateTrigger(voltage);

            if (trigger)
                flashSecondsRemaining = FlashDurationSeconds;
            else if (flashSecondsRemaining > 0)
                flashSecondsRemaining = std::max<float>(0, flashSecondsRemaining - 1/sampleRateHz);

            return trigger;
        }

        bool lit() const
        {
            return flashSecondsRemaining > 0;
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
        bool isAttenuverter = false;
        bool isLowSensitive = false;
    };

    struct SapphirePortInfo
    {
        bool flipVoltagePolarity = false;
    };

    enum class InputStereoMode
    {
        LeftRight,
        Left2,
        Right2,
    };


    struct ControlGroupIds
    {
        int paramId;
        int attenId;
        int cvInputId;

        ControlGroupIds()
            : paramId(-1)
            , attenId(-1)
            , cvInputId(-1)
            {}

        explicit ControlGroupIds(int _paramId, int _attenId, int _cvInputId)
            : paramId(_paramId)
            , attenId(_attenId)
            , cvInputId(_cvInputId)
            {}
    };


    struct SapphireModule : public Module
    {
        static std::vector<SapphireModule*> All;

        Tricorder::VectorSender vectorSender;
        Tricorder::VectorReceiver vectorReceiver;
        std::vector<SapphireParamInfo> paramInfo;
        std::vector<SapphirePortInfo> outputPortInfo;
        bool provideStereoSplitter = false;     // derived class must opt in by setting this flag during constructor
        bool enableStereoSplitter{};
        bool provideStereoMerge = false;
        bool enableStereoMerge{};
        InputStereoMode inputStereoMode = InputStereoMode::LeftRight;
        float autoResetVoltageThreshold = 100;
        bool enableLimiterWarning = true;
        int limiterRecoveryCountdown = 0;      // positive integer indicates we are recovering from NAN/INF
        int currentChannelCount = 0;           // used only by modules that display a channel count on the panel
        bool shouldClearTricorder = false;     // used only by modules that send vectors to Tricorder for display
        bool provideModelResampler = false;
        int modelSampleRate = 0;
        bool hideLeftBorder  = false;
        bool hideRightBorder = false;
        bool neonMode = false;
        bool includeNeonModeMenuItem = true;
        DcRejectQuantity *dcRejectQuantity = nullptr;

        explicit SapphireModule(std::size_t nParams, std::size_t nOutputPorts)
            : vectorSender(*this)
            , vectorReceiver(*this)
            , paramInfo(nParams)
            , outputPortInfo(nOutputPorts)
            {}

        float cvGetControlValue(int paramId, int attenId, float cv, float minValue = 0, float maxValue = 1)
        {
            float slider = params.at(paramId).getValue();
            // When the attenuverter is set to 100%, and the cv is +5V, we want
            // to swing a slider that is all the way down (minSlider)
            // to act like it is all the way up (maxSlider).
            // Thus we allow the complete range of control for any CV whose
            // range is [-5, +5] volts.
            float attenu = params.at(attenId).getValue();
            if (isLowSensitive(attenId))
                attenu /= AttenuverterLowSensitivityDenom;
            slider += attenu*(cv / 5)*(maxValue - minValue);
            return std::clamp(slider, minValue, maxValue);
        }

        float cvGetVoltPerOctave(int paramId, int attenId, float cv, float minValue, float maxValue)
        {
            // Make it easy for a human to use this control voltage for V/OCT.
            // Just turn the attenuverter to +100%, disable "low sensitivity",
            // and each 1V change in CV is reflected in the return value (within clamping limits).
            float slider = params.at(paramId).getValue();
            float attenu = params.at(attenId).getValue();
            if (isLowSensitive(attenId))
                attenu /= AttenuverterLowSensitivityDenom;
            slider += attenu * cv;
            return std::clamp(slider, minValue, maxValue);
        }

        float controlGroupRawCv(int channel, float& cv, const ControlGroupIds& ids, float minValue, float maxValue, float cvScalar = 1)
        {
            nextChannelInputVoltage(cv, ids.cvInputId, channel);
            return cvGetVoltPerOctave(ids.paramId, ids.attenId, cv * cvScalar, minValue, maxValue);
        }

        float controlGroupAmpCv(int channel, float& cv, const ControlGroupIds& ids, float minValue, float maxValue)
        {
            nextChannelInputVoltage(cv, ids.cvInputId, channel);
            return cvGetControlValue(ids.paramId, ids.attenId, cv, minValue, maxValue);
        }

        float getControlValue(int paramId, int attenId, int inputId, float minValue = 0, float maxValue = 1)
        {
            float cv = inputs.at(inputId).getVoltageSum();
            return cvGetControlValue(paramId, attenId, cv, minValue, maxValue);
        }

        float getControlValue(const ControlGroupIds& ids, float minValue, float maxValue)
        {
            return getControlValue(ids.paramId, ids.attenId, ids.cvInputId, minValue, maxValue);
        }

        float getControlValueCustom(const ControlGroupIds& ids, float minValue, float maxValue, float cvScalar)
        {
            return getControlValueVoltPerOctave(ids.paramId, ids.attenId, ids.cvInputId, minValue, maxValue, cvScalar);
        }

        float getControlValueVoltPerOctave(int paramId, int attenId, int inputId, float minValue = 0, float maxValue = 1, float cvScalar = 1)
        {
            float cv = inputs.at(inputId).getVoltageSum();
            return cvGetVoltPerOctave(paramId, attenId, cvScalar * cv, minValue, maxValue);
        }

        void defineAttenuverterId(int attenId)
        {
            // We need to know which parameter IDs are actually for attenuverter knobs.
            // This is a hook for passing in that information.
            paramInfo.at(attenId).isAttenuverter = true;
        }

        bool isAttenuverter(int paramId) const
        {
            return paramInfo.at(paramId).isAttenuverter;
        }

        bool isLowSensitive(int attenId) const
        {
            return paramInfo.at(attenId).isLowSensitive;
        }

        bool *lowSensitiveFlag(int attenId)
        {
            return &paramInfo.at(attenId).isLowSensitive;
        }

        void toggleAllSensitivity()
        {
            // Find all attenuverter knobs and toggle their low-sensitivity state together.
            const int nparams = static_cast<int>(paramInfo.size());
            int countEnabled = 0;
            int countDisabled = 0;
            for (int paramId = 0; paramId < nparams; ++paramId)
                if (isAttenuverter(paramId))
                    isLowSensitive(paramId) ? ++countEnabled : ++countDisabled;

            // Let the knobs "vote". If a supermajority are enabled,
            // then we turn them all off.
            // Otherwise we turn them all on.
            const bool toggle = (countEnabled <= countDisabled);
            for (int paramId = 0; paramId < nparams; ++paramId)
                if (isAttenuverter(paramId))
                    *lowSensitiveFlag(paramId) = toggle;
        }

        MenuItem* createToggleAllSensitivityMenuItem()
        {
            return createMenuItem(
                "Toggle sensitivity on all attenuverters",
                "",
                [this]{ toggleAllSensitivity(); }
            );
        }

        MenuItem* createStereoSplitterMenuItem()
        {
            return createBoolPtrMenuItem<bool>("Enable input stereo splitter", "", &enableStereoSplitter);
        }

        MenuItem* createStereoMergeMenuItem()
        {
            return createBoolPtrMenuItem<bool>("Send polyphonic stereo to L output", "", &enableStereoMerge);
        }

        void sendVector(float x, float y, float z, bool reset)
        {
            vectorSender.sendVector(x, y, z, reset);
        }

        bool isVectorReceiverConnectedOnRight() const
        {
            return vectorSender.isVectorReceiverConnectedOnRight();
        }

        bool isVectorSenderConnectedOnLeft() const
        {
            return vectorReceiver.isVectorSenderConnectedOnLeft();
        }

        void onAdd(const AddEvent& e) override
        {
            if (std::find(All.begin(), All.end(), this) == All.end())
                All.push_back(this);
        }

        void onRemove(const RemoveEvent& e) override
        {
            // Delete all instances of `this` pointer in `All`.
            All.erase(std::remove(All.begin(), All.end(), this), All.end());
        }

        json_t* dataToJson() override
        {
            json_t* root = json_object();

            // We have to save/restore the attenuverter sensitivity settings.
            // Represent the settings by saving a list of all the integer attenuverter
            // IDs that have low sensitivity enabled.
            const int nparams = static_cast<int>(paramInfo.size());
            json_t* aList = json_array();
            for (int attenId = 0; attenId < nparams; ++attenId)
                if (isLowSensitive(attenId))
                    json_array_append(aList, json_integer(attenId));
            json_object_set_new(root, "lowSensitivityAttenuverters", aList);

            json_t* oList = json_array();
            const int nOutputPorts = static_cast<int>(outputPortInfo.size());
            for (int outputId = 0; outputId < nOutputPorts; ++outputId)
                if (getVoltageFlipEnabled(outputId))
                    json_array_append(oList, json_integer(outputId));
            json_object_set_new(root, "voltageFlippedOutputPorts", oList);

            if (provideStereoSplitter)
                json_object_set_new(root, "enableStereoSplitter", json_boolean(enableStereoSplitter));

            if (provideStereoMerge)
                json_object_set_new(root, "enableStereoMerge", json_boolean(enableStereoMerge));

            if (provideModelResampler)
                json_object_set_new(root, "modelSampleRate", json_integer(modelSampleRate));

            json_object_set_new(root, "neonMode", json_boolean(neonMode));

            if (dcRejectQuantity)
                dcRejectQuantity->save(root, "dcRejectFrequency");

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
                paramInfo.at(attenId).isLowSensitive = false;

            json_t* aList = json_object_get(root, "lowSensitivityAttenuverters");
            if (aList)
            {
                std::size_t listLength = static_cast<int>(json_array_size(aList));
                for (std::size_t listIndex = 0; listIndex < listLength; ++listIndex)
                {
                    json_t *item = json_array_get(aList, listIndex);
                    if (json_is_integer(item))
                    {
                        int attenId = static_cast<int>(json_integer_value(item));
                        if (attenId >= 0 && attenId < nparams)
                            paramInfo.at(attenId).isLowSensitive = true;
                    }
                }
            }

            const int nOutputs = static_cast<int>(outputPortInfo.size());
            for (int outputId = 0; outputId < nOutputs; ++outputId)
                outputPortInfo.at(outputId).flipVoltagePolarity = false;

            json_t* oList = json_object_get(root, "voltageFlippedOutputPorts");
            if (oList)
            {
                std::size_t listLength = static_cast<int>(json_array_size(oList));
                for (std::size_t listIndex = 0; listIndex < listLength; ++listIndex)
                {
                    json_t *item = json_array_get(oList, listIndex);
                    if (json_is_integer(item))
                    {
                        int outputId = static_cast<int>(json_integer_value(item));
                        if (outputId >= 0 && outputId < nOutputs)
                            outputPortInfo.at(outputId).flipVoltagePolarity = true;
                    }
                }
            }

            if (provideStereoSplitter)
            {
                json_t *splitFlag = json_object_get(root, "enableStereoSplitter");
                enableStereoSplitter = json_is_true(splitFlag);
            }

            if (provideStereoMerge)
            {
                json_t *mergeFlag = json_object_get(root, "enableStereoMerge");
                enableStereoMerge = json_is_true(mergeFlag);
            }

            if (provideModelResampler)
            {
                json_t* rate = json_object_get(root, "modelSampleRate");
                if (json_is_integer(rate))
                    modelSampleRate = json_integer_value(rate);
            }

            json_t* jsNeonMode = json_object_get(root, "neonMode");
            if (json_is_boolean(jsNeonMode))
                neonMode = json_boolean_value(jsNeonMode);

            if (dcRejectQuantity)
                dcRejectQuantity->load(root, "dcRejectFrequency");
        }

        virtual void tryCopySettingsFrom(SapphireModule* other)
        {
        }

        void loadStereoInputs(float& inLeft, float& inRight, int leftPortIndex, int rightPortIndex)
        {
            const int ncl = inputs.at(leftPortIndex ).channels;
            const int ncr = inputs.at(rightPortIndex).channels;

            if (enableStereoSplitter)
            {
                // Special option: stereo given to either input port,
                // but with the other port disconnected, results in a stereo splitter.

                if (ncl >= 2 && ncr == 0)
                {
                    inLeft  = inputs.at(leftPortIndex).getVoltage(0);
                    inRight = inputs.at(leftPortIndex).getVoltage(1);
                    inputStereoMode = InputStereoMode::Left2;
                    return;
                }

                if (ncr >= 2 && ncl == 0)
                {
                    inLeft  = inputs.at(rightPortIndex).getVoltage(0);
                    inRight = inputs.at(rightPortIndex).getVoltage(1);
                    inputStereoMode = InputStereoMode::Right2;
                    return;
                }
            }

            // Assume separate data fed to each input port.
            inputStereoMode = InputStereoMode::LeftRight;

            inLeft  = inputs.at(leftPortIndex ).getVoltageSum();
            inRight = inputs.at(rightPortIndex).getVoltageSum();

            // But if only one of the two input ports has a cable,
            // split that cable's voltage equally between the left and right inputs.
            // This is "mono" mode.

            if (ncl > 0 && ncr == 0)
                inLeft = inRight = inLeft / 2;
            else if (ncr > 0 && ncl == 0)
                inLeft = inRight = inRight / 2;
        }

        void writeStereoOutputs(float outLeft, float outRight, int leftPortIndex, int rightPortIndex)
        {
            if (enableStereoMerge)
            {
                outputs.at(leftPortIndex).setChannels(2);
                outputs.at(leftPortIndex).setVoltage(outLeft,  0);
                outputs.at(leftPortIndex).setVoltage(outRight, 1);

                outputs.at(rightPortIndex).setChannels(1);
                outputs.at(rightPortIndex).setVoltage(0);
            }
            else
            {
                outputs.at(leftPortIndex).setChannels(1);
                outputs.at(leftPortIndex ).setVoltage(outLeft);

                outputs.at(rightPortIndex).setChannels(1);
                outputs.at(rightPortIndex).setVoltage(outRight);
            }
        }

        ChannelCountQuantity* configChannelCount(int paramId, int defaultChannelCount)
        {
            return configParam<ChannelCountQuantity>(
                paramId,
                0.5f,
                16.5f,
                static_cast<float>(defaultChannelCount),
                "Output channels"
            );
        }

        int numOutputChannels(int numInputs, int minChannels)
        {
            int nc = minChannels;
            for (int i = 0; i < numInputs; ++i)
                nc = std::max(nc, inputs.at(i).getChannels());
            return std::clamp(nc, 0, PORT_MAX_CHANNELS);
        }

        float nextChannelInputVoltage(float& voltage, int inputId, int channel)
        {
            Input& input = inputs.at(inputId);
            if (channel >= 0 && channel < input.getChannels())
                voltage = input.getVoltage(channel);
            return voltage;
        }

        void configStereoInputs(int leftPortId, int rightPortId, const std::string& suffix)
        {
            configInput(leftPortId, "Left " + suffix);
            configInput(rightPortId, "Right " + suffix);
        }

        void configStereoOutputs(int leftPortId, int rightPortId, const std::string& suffix)
        {
            configOutput(leftPortId, "Left " + suffix);
            configOutput(rightPortId, "Right " + suffix);
        }

        ParamQuantity* configAtten(int attenId, const std::string& name)
        {
            return configParam(attenId, -1, +1, 0, name + " attenuverter", "%", 0, 100);
        }

        PortInfo* configCvInput(int cvInputId, const std::string& name)
        {
            return configInput(cvInputId, name + " CV");
        }

        void configAttenCv(int attenId, int cvInputId, const std::string& name)
        {
            configAtten(attenId, name);
            configCvInput(cvInputId, name);
        }

        void configControlGroup(
            const std::string& name,
            int paramId,
            int attenId,
            int cvInputId,
            float minValue = -1,
            float maxValue = +1,
            float defValue =  0,
            std::string unit = "",
            float displayBase = 0,
            float displayMultiplier = 1)
        {
            configParam(paramId, minValue, maxValue, defValue, name, unit, displayBase, displayMultiplier);
            configAttenCv(attenId, cvInputId, name);
        }

        void configToggleGroup(int inputId, int buttonParamId, const std::string& buttonCaption, const std::string& inputPrefix)
        {
            configButton(buttonParamId, buttonCaption);
            configInput(inputId, inputPrefix);
        }

        bool getVoltageFlipEnabled(int outputId) const
        {
            return
                (outputId >= 0) &&
                (outputId < static_cast<int>(outputPortInfo.size())) &&
                outputPortInfo[outputId].flipVoltagePolarity;
        }

        void setVoltageFlipEnabled(int outputId, bool state)
        {
            bool& flip = outputPortInfo.at(outputId).flipVoltagePolarity;
            if (flip != state)
            {
                flip = state;
                shouldClearTricorder = true;
            }
        }

        float setFlippableOutputVoltage(int outputId, float originalVoltage)
        {
            float voltage = originalVoltage;
            if (getVoltageFlipEnabled(outputId))
                voltage = -voltage;
            outputs.at(outputId).setVoltage(voltage);
            return voltage;
        }

        bool isBadOutput(float output) const
        {
            return !std::isfinite(output) || std::abs(output) > autoResetVoltageThreshold;
        }

        bool checkOutputs(float sampleRateHz, float outputArray[], int arrayLength)
        {
            // Is the output getting out of control? Or even NAN?
            bool bad = false;
            for (int i = 0; i < arrayLength; ++i)
                if (isBadOutput(outputArray[i]))
                    bad = true;

            if (bad)
            {
                // Silence the output for one second.
                limiterRecoveryCountdown = static_cast<int>(sampleRateHz);

                // Start the silence on this sample.
                for (int i = 0; i < arrayLength; ++i)
                    outputArray[i] = 0;
            }

            return bad;
        }

        virtual double getAgcDistortion()
        {
            return 0.0;     // no distortion
        }

        virtual NVGcolor getWarningColor()
        {
            if (limiterRecoveryCountdown > 0)
            {
                // The module is recovering from non-finite (NAN/infinite) output.
                // Inflict an obnoxiously bright pink OUTPUT knob glow on the user!
                return nvgRGBA(0xff, 0x00, 0xff, 0xb0);
            }

            const double distortion = getAgcDistortion();
            if (!enableLimiterWarning || distortion <= 0.0)
                return nvgRGBA(0, 0, 0, 0);     // no warning light

            double decibels = 20.0 * std::log10(1.0 + distortion);
            // On Cardinal builds, one of the environments uses a compiler
            // option to convert `double` constants to `float`.
            // This causes a compiler error calling std::clamp()
            // unless we "lock in" the types.
            const double minScale = 0;
            const double maxScale = 1;
            double scale = std::clamp(decibels / 24.0, minScale, maxScale);

            int red   = limiterColorComponent(scale, 0x90, 0xff);
            int green = limiterColorComponent(scale, 0x20, 0x50);
            int blue  = 0x00;
            int alpha = 0x70;

            return nvgRGBA(red, green, blue, alpha);
        }

        static int limiterColorComponent(double scale, int lo, int hi)
        {
            return std::clamp(static_cast<int>(round(lo + scale*(hi-lo))), lo, hi);
        }

        void addDcRejectQuantity(int paramId, float defaultFrequencyHz)
        {
            assert(dcRejectQuantity == nullptr);    // do not initialize more than once

            dcRejectQuantity = configParam<DcRejectQuantity>(
                paramId,
                DC_REJECT_MIN_FREQ,
                DC_REJECT_MAX_FREQ,
                defaultFrequencyHz,
                "DC reject cutoff",
                " Hz"
            );

            dcRejectQuantity->value = defaultFrequencyHz;
            dcRejectQuantity->changed = true;
        }

        AgcLevelQuantity* makeAgcLevelQuantity(
            int   paramId,
            float levelMin   =  1.0,
            float levelDef   =  4.0,
            float levelMax   = 10.0,
            float disableMin = 10.1,
            float disableMax = 10.2)
        {
            AgcLevelQuantity *agcLevelQuantity = configParam<AgcLevelQuantity>(
                paramId,
                levelMin,
                disableMax,
                levelDef,
                "Output limiter"
            );

            agcLevelQuantity->value = levelDef;
            agcLevelQuantity->levelMin = levelMin;
            agcLevelQuantity->levelMax = levelMax;
            agcLevelQuantity->disableMin = disableMin;

            return agcLevelQuantity;
        }

        bool updateTriggerGroup(
            float sampleRateHz,
            AnimatedTriggerReceiver& receiver,
            int inputId,
            int buttonParamId,
            int buttonLightId)
        {
            Input& input  = inputs.at(inputId);
            Param& button = params.at(buttonParamId);

            // We treat the button state as if it is a 0V/10V input voltage.
            // Then we boolean-OR the logic levels of the input port and button.
            float inputVoltage = input.getVoltageSum();
            if (button.getValue() > 0)
                inputVoltage = 10;

            bool trigger = receiver.updateTrigger(inputVoltage, sampleRateHz);
            setLightBrightness(buttonLightId, receiver.lit());
            return trigger;
        }

        void setLightBrightness(int lightId, bool lit)
        {
            if (lightId >= 0)
                lights.at(lightId).setBrightness(lit ? 1.0f : 0.06f);
        }
    };


    enum class ToggleGroupMode
    {
        Gate,
        Trigger,
        LEN
    };


    class ToggleGroup
    {
    private:
        SapphireModule* smod = nullptr;
        const char *jsonKey = nullptr;
        int inputId = -1;
        int buttonParamId = -1;
        int buttonLightId = -1;
        GateTriggerReceiver receiver;
        ToggleGroupMode mode{};

    public:
        ToggleGroup()
        {
            initialize();
        }

        void initialize()
        {
            receiver.initialize();
            mode = ToggleGroupMode::Gate;
        }

        void config(
            SapphireModule* _smod,
            const char *_jsonKey,
            int _inputId,
            int _buttonParamId,
            int _buttonLightId,
            const std::string& buttonCaption,
            const std::string& inputPrefix)
        {
            smod = _smod;
            jsonKey = _jsonKey;
            inputId = _inputId;
            buttonParamId = _buttonParamId;
            buttonLightId = _buttonLightId;
            if (_smod)
                _smod->configToggleGroup(_inputId, _buttonParamId, buttonCaption, inputPrefix);
        }

        void jsonSave(json_t* root)
        {
            // Create a child object for this toggle group.
            // Future-proofing: inside it we set whatever fields we want.
            json_t* child = json_object();
            json_object_set_new(root, jsonKey, child);

            // For now, the mode is the only thing we need.
            jsonSetEnum(child, "mode", mode);
        }

        void jsonLoad(json_t* root)
        {
            json_t* child = json_object_get(root, jsonKey);
            if (json_is_object(child))
            {
                jsonLoadEnum(child, "mode", mode);
            }
        }

        bool process()
        {
            if (!smod || inputId<0 || buttonParamId<0 || buttonLightId<0)
                return false;

            Input& input = smod->inputs.at(inputId);
            Param& button = smod->params.at(buttonParamId);
            bool portActive = receiver.updateGate(input.getVoltageSum());
            bool buttonActive = (button.getValue() > 0);

            // Allow the button to toggle the gate state, so the gate can be active-low or active-high.
            bool active = portActive ^ buttonActive;
            smod->setLightBrightness(buttonLightId, active);
            return active;
        }
    };


    class WarningLightWidget : public LightWidget
    {
    private:
        SapphireModule *alModule;

    public:
        explicit WarningLightWidget(SapphireModule *_alModule)
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


    struct SapphirePort : app::SvgPort
    {
        bool allowsVoltageFlip = false;
        SapphireModule* module = nullptr;
        int outputId = -1;

        SapphirePort()
        {
            setSvg(Svg::load(asset::plugin(pluginInstance, "res/port.svg")));
        }

        void appendContextMenu(ui::Menu* menu) override
        {
            app::SvgPort::appendContextMenu(menu);
            if (module && allowsVoltageFlip && (outputId >= 0))
            {
                menu->addChild(new MenuSeparator);

                menu->addChild(createBoolMenuItem(
                    "Flip voltage polarity",
                    "",
                    [=]()
                    {
                        return module->getVoltageFlipEnabled(outputId);
                    },
                    [=](bool state)
                    {
                        module->setVoltageFlipEnabled(outputId, state);
                    }
                ));
            }
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


    enum class ExpanderDirection
    {
        Left,
        Right,
    };


    inline bool IsModelType(const ModuleWidget* widget, const Model* model)
    {
        return widget && model && widget->model == model;
    }


    inline bool IsModelType(const Module* module, const Model* model)
    {
        // This function is useful for recognizing our own modules in
        // an expander chain.
        // Example: IsModelType(rightExpander.module, modelSapphireTricorder)
        return module && model && module->model == model;
    }

    template <typename enum_t>
    ui::MenuItem* createEnumMenuItem(
        std::string text,
        std::vector<std::string> labels,
        enum_t& option)
    {
        assert(labels.size() == static_cast<std::size_t>(enum_t::LEN));

        return createIndexSubmenuItem(
            text,
            labels,
            [&option]() { return static_cast<std::size_t>(option); },
            [&option](size_t index) { option = static_cast<enum_t>(index); }
        );
    }
}


// Keep this in the global namespace, not inside "Sapphire".
template <class TModule, class TModuleWidget>
rack::plugin::Model* createSapphireModel(
    std::string slug,
    Sapphire::ExpanderRole roles)
{
    Model *model = rack::createModel<TModule, TModuleWidget>(slug);
    Sapphire::ModelInfo::insert(model, roles);
    return model;
}
