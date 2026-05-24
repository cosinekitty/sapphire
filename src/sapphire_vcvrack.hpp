// Sapphire for VCV Rack 2, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire
#pragma once
#include "plugin.hpp"
#include "sapphire_crossfader.hpp"
#include "sapphire_engine.hpp"
#include "sapphire_attenuverter_context.hpp"
#include "sapphire_checkfloat.hpp"

namespace Sapphire
{
    struct SapphireModule;
    struct SapphireWidget;

    NVGcolor VoltageColor(float voltage);

    inline void InvokeAction(history::Action* action)
    {
        if (action)
        {
            action->redo();
            APP->history->push(action);
        }
    }

    inline int VcvSafeChannelCount(int count)
    {
        return std::clamp<int>(count, 0, PORT_MAX_CHANNELS);
    }

    ModuleWidget* FindWidgetForId(int64_t moduleId);
    Module* FindModuleForId(int64_t moduleId);

    template <typename widget_t = SapphireWidget>
    widget_t* FindSapphireWidget(int64_t moduleId)
    {
        return dynamic_cast<widget_t*>(FindWidgetForId(moduleId));
    }

    template <typename module_t = SapphireModule>
    module_t* FindSapphireModule(int64_t moduleId)
    {
        return dynamic_cast<module_t*>(FindModuleForId(moduleId));
    }

    enum class ExpanderRole
    {
        None            = 0,
        VectorSender    = 0x01,
        VectorReceiver  = 0x02,
        ChaosOpSender   = 0x04,     // Chaops
        ChaosOpReceiver = 0x08,     // Frolic, Glee, Lark, Zoo
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
            if (module)
                if (ModelInfo* info = search(module->model))
                    return 0 != (static_cast<int>(info->roles) & static_cast<int>(role));

            return false;
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
        if (json_t* js = json_object_get(root, key); json_is_integer(js))
            value = static_cast<enum_t>(json_integer_value(js));
    }

    inline void jsonSetInt(json_t* root, const char *key, int value)
    {
        jsonSetEnum<int>(root, key, value);
    }

    inline void jsonLoadInt(json_t* root, const char *key, int& value)
    {
        jsonLoadEnum<int>(root, key, value);
    }

    inline void jsonSetBool(json_t* root, const char *key, bool value)
    {
        json_object_set_new(root, key, json_boolean(value));
    }

    inline void jsonLoadBool(json_t* root, const char* key, bool& value)
    {
        if (json_t* js = json_object_get(root, key); json_is_boolean(js))
            value = json_boolean_value(js);
    }

    inline void jsonSetDouble(json_t* root, const char *key, double value)
    {
        json_object_set_new(root, key, json_real(value));
    }

    inline void jsonLoadDouble(json_t* root, const char *key, double& value)
    {
        if (json_t* js = json_object_get(root, key); json_is_number(js))
            value = json_real_value(js);
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
        static constexpr unsigned MemoryCount = 16;

        struct Message
        {
            bool store  = false;
            bool recall = false;
            bool freeze = false;
            unsigned memoryIndex = 0;       // 0..(MemoryCount-1)
            float morph = 0;
            float cruise = 0;
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


    struct StereoButtonQuantity : ParamQuantity
    {
        std::string getDisplayValueString() override
        {
            return (getValue() > 0.5f) ? "ENABLED" : "DISABLED";
        }
    };


    template <typename enum_t>
    struct ChangeEnumAction : history::Action
    {
        enum_t& option;
        enum_t  oldValue;
        enum_t  newValue;

        explicit ChangeEnumAction(enum_t& _option, enum_t _newValue, const std::string& _actionName)
            : option(_option)
            , oldValue(_option)
            , newValue(_newValue)
        {
            name = _actionName;
        }

        void undo() override
        {
            option = oldValue;
        }

        void redo() override
        {
            option = newValue;
        }
    };


    template <typename enum_t>
    MenuItem* CreateChangeEnumMenuItem(
        std::string text,
        std::vector<std::string> labels,
        const std::string& actionName,
        enum_t& option)
    {
        assert(labels.size() == static_cast<std::size_t>(enum_t::LEN));

        return createIndexSubmenuItem(
            text,
            labels,
            [&option]()
            {
                return static_cast<std::size_t>(option);
            },
            [&option, actionName](size_t index)
            {
                const enum_t newValue = static_cast<enum_t>(index);
                if (newValue != option)
                    InvokeAction(new ChangeEnumAction<enum_t>(option, newValue, actionName));
            }
        );
    }


    struct SliderAction : history::Action
    {
        const int64_t moduleId;
        const int paramId;
        const float oldValue;
        const float newValue;

        explicit SliderAction(int64_t _moduleId, int _paramId, float _oldValue, float _newValue, const std::string& _name)
            : moduleId(_moduleId)
            , paramId(_paramId)
            , oldValue(_oldValue)
            , newValue(_newValue)
        {
            name = _name;
        }

        void setParameterValue(float value);

        void undo() override
        {
            setParameterValue(oldValue);
        }

        void redo() override
        {
            setParameterValue(newValue);
        }
    };


    inline int SnapChannelCount(float value)
    {
        const int n = static_cast<int>(std::round(value));
        return std::clamp(n, 1, 16);
    }


    struct ChannelCountQuantity : SapphireQuantity
    {
        int getDesiredChannelCount() const
        {
            return SnapChannelCount(value);
        }

        std::string getDisplayValueString() override
        {
            return string::f("%d", getDesiredChannelCount());
        }
    };


    struct SapphireSlider : Slider      // adds undo/redo support
    {
        const int64_t moduleId;
        const int paramId;
        const float startValue;
        float currentValue{};
        const std::string actionName;

        explicit SapphireSlider(SapphireQuantity* _quantity, const std::string& _actionName)
            : moduleId(_quantity->module->id)
            , paramId(_quantity->paramId)
            , startValue(_quantity->getValue())
            , currentValue(_quantity->getValue())
            , actionName(_actionName)
        {
            quantity = _quantity;
            box.size.x = 200;
        }

        void onRemove(const RemoveEvent& args) override     // called when the menu closes
        {
            const float snapStartValue = snap(startValue);
            const float snapFinalValue = snap(currentValue);
            if (snapFinalValue != snapStartValue)
                APP->history->push(new SliderAction(moduleId, paramId, snapStartValue, snapFinalValue, actionName));
            Slider::onRemove(args);
        }

        void step() override
        {
            Slider::step();
            currentValue = quantity->getValue();
        }

        virtual float snap(float value) const
        {
            return value;
        }
    };


    struct ChannelCountSlider : SapphireSlider
    {
        ChannelCountQuantity* ccq{};

        explicit ChannelCountSlider(ChannelCountQuantity *_quantity)
            : SapphireSlider(_quantity, "adjust output channel count")
            , ccq(_quantity)
            {}

        float snap(float value) const override
        {
            return SnapChannelCount(value);
        }

        void draw(const DrawArgs& args) override
        {
            // [Don Cross] Copied and modified from: Rack/src/ui/Slider.cpp

            BNDwidgetState state = BND_DEFAULT;
            if (APP->event->hoveredWidget == this)
                state = BND_HOVER;
            if (APP->event->draggedWidget == this)
                state = BND_ACTIVE;

            // Render the slider snapped to the nearest integer number of channels.
            // Compensate for rescaling: progress ranges 0 to 1, but channels from 1 to 16.
            // There is also a 0.5 "buffer" in the channel count at the top and bottom of the range.
            float progress = std::clamp((ccq->getDesiredChannelCount() - 0.5f) / 16.0f, 0.0f, 1.0f);
            const std::string text = ccq->getString();

            // If parent is a Menu, make corners sharp
            auto parentMenu = dynamic_cast<const Menu*>(getParent());
            int flags = parentMenu ? BND_CORNER_ALL : BND_CORNER_NONE;
            bndSlider(args.vg, 0.0, 0.0, box.size.x, box.size.y, flags, state, progress, text.c_str(), nullptr);
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


    struct DcRejectSlider : SapphireSlider
    {
        explicit DcRejectSlider(DcRejectQuantity *_quantity)
            : SapphireSlider(_quantity, "adjust DC reject corner frequency")
            {}
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
        bool fallingEdge{};

    public:
        bool isGateActive() const { return gate; }
        bool isTriggerActive() const { return trigger; }
        bool isFallingEdge() const { return fallingEdge; }

        void initialize()
        {
            prevVoltage = 0;
            gate = false;
            trigger = false;
            fallingEdge = false;
        }

        void update(float voltage)
        {
            trigger = false;
            fallingEdge = false;

            if (prevVoltage < 1 && voltage >= 1)
            {
                trigger = !gate;
                gate = true;
            }
            else if (prevVoltage >= 0.1 && voltage < 0.1)
            {
                fallingEdge = gate;
                gate = false;
            }

            prevVoltage = voltage;
        }
    };


    constexpr double FlashDurationSeconds = 0.05;


    class AnimatedTriggerReceiver
    {
    private:
        GateTriggerReceiver tr;
        double flashSecondsRemaining = 0;

    public:
        void initialize()
        {
            tr.initialize();
            flashSecondsRemaining = 0;
        }

        bool isTriggerActive() const
        {
            return tr.isTriggerActive();
        }

        bool isFallingEdge() const
        {
            return tr.isFallingEdge();
        }

        void update(float voltage, double sampleRateHz)
        {
            tr.update(voltage);
            if (tr.isTriggerActive())
                flashSecondsRemaining = FlashDurationSeconds;
            else if (flashSecondsRemaining > 0)
                flashSecondsRemaining = std::max<double>(0, flashSecondsRemaining - 1/sampleRateHz);
        }

        bool lit() const
        {
            return flashSecondsRemaining > 0;
        }
    };


    class TriggerSender
    {
    private:
        double elapsed = 0;
        bool isFiring = false;

    public:
        void initialize()
        {
            elapsed = 0;
            isFiring = false;
        }

        float process(double sampleRateHz, bool fire)
        {
            if (fire)
            {
                elapsed = 0;
                isFiring = true;
            }
            if (isFiring)
            {
                if (elapsed >= 0.001)   // hold the gate high for at least 1 millisecond.
                    isFiring = false;
                elapsed += 1/sampleRateHz;
                return 10;  // high gate voltage
            }
            return 0;   // low gate voltage
        }
    };


    struct SapphireParamInfo
    {
        bool isAttenuverter = false;
        SapphireAttenuverterContext context;
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


    struct SensitivityState
    {
        const int paramId;
        const bool lowSensitivity;

        explicit SensitivityState(int _paramId, bool _lowSensitivity)
            : paramId(_paramId)
            , lowSensitivity(_lowSensitivity)
        {
        }
    };


    struct ToggleAllSensitivityAction : history::Action
    {
        int64_t moduleId = -1;
        std::vector<SensitivityState> prevStateList;

        explicit ToggleAllSensitivityAction(SapphireModule* sapphireModule);
        void redo() override;
        void undo() override;
    };


    class RemovalSubscriber
    {
    public:
        virtual void disconnect() = 0;
    };


    class EnvelopeFollower
    {
    private:
        float prevSampleRate{};
        float envAttack{};
        float envDecay{};
        float envelope{};
        LoHiPassFilter<float> filter;

    public:
        void initialize()
        {
            envelope = 0;
            filter.Reset();
            filter.SetCutoffFrequency(80);
        }

        float update(float signal, int sampleRate)
        {
            // Based on Surge XT Tree Monster's envelope follower:
            // https://github.com/surge-synthesizer/sst-effects/blob/main/include/sst/effects-shared/TreemonsterCore.h
            if (sampleRate != prevSampleRate)
            {
                prevSampleRate = sampleRate;
                envAttack = std::pow(0.01, 1.0 / (0.003*sampleRate));
                envDecay  = std::pow(0.01, 1.0 / (0.150*sampleRate));
            }
            float v = std::abs(signal);
            float k = (v > envelope) ? envAttack : envDecay;
            envelope = k*(envelope - v) + v;

            const float correction = (5.0 / 4.783);     // experimentally derived using sinewave input
            filter.Update(correction * envelope, sampleRate);
            return filter.LoPass();
        }
    };


    struct EnvelopeFollowerFeature
    {
        bool enabled{};
        bool polyphonicOutput{};
        bool duck{};
        Crossfader envDuckFader;
        EnvelopeFollower follower[PORT_MAX_CHANNELS];

        void initialize()
        {
            polyphonicOutput = false;
            duck = false;
            envDuckFader.snapToFront();
            for (int c = 0; c < PORT_MAX_CHANNELS; ++c)
                follower[c].initialize();
        }

        void loadJson(json_t* root)
        {
            if (enabled)
            {
                jsonLoadBool(root, "polyphonicEnvelopeOutput", polyphonicOutput);
                jsonLoadBool(root, "duck", duck);
            }
        }

        void saveJson(json_t* root)
        {
            if (enabled)
            {
                jsonSetBool(root, "polyphonicEnvelopeOutput", polyphonicOutput);
                jsonSetBool(root, "duck", duck);
            }
        }

        void copyFrom(const EnvelopeFollowerFeature& other)
        {
            polyphonicOutput = other.polyphonicOutput;
            duck = other.duck;
        }

        float scaleEnvelope(float env, float sampleRateHz)
        {
            constexpr float limit = 10;      // maximum voltage
            float scale = BicubicLimiter(env, limit);
            envDuckFader.setTarget(duck);
            return envDuckFader.process(sampleRateHz, scale, limit - scale);
        }
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
        AgcLevelQuantity *agcLevelQuantity = nullptr;
        bool enableLimiterMenuItems = true;     // default for older modules; newer modules add menu items to knobs only
        std::vector<RemovalSubscriber*> removalSubscriberList;
        EnvelopeFollowerFeature envelopeFollower;
        bool redAlert = false;      // one-shot trigger for widget to start a "red alert" splash
        bool shouldOfferFireDrill = false;      // derived module can opt in to add internal testing menu item
        bool fireDrillTrigger = false;

        explicit SapphireModule(std::size_t nParams, std::size_t nOutputPorts)
            : vectorSender(*this)
            , vectorReceiver(*this)
            , paramInfo(nParams)
            , outputPortInfo(nOutputPorts)
            {}

        virtual ~SapphireModule()
        {
            // Any lingering removal-subscribers indicates memory corruption is possible very soon.
            // This assert should only fail if somebody destructs a SapphireModule without calling
            // its onRemove() method first.
            assert(removalSubscriberList.empty());
        }

        void onReset(const ResetEvent& e) override
        {
            Module::onReset(e);
            SapphireModule_initialize();
        }

        void onAdd(const AddEvent& e) override;
        void onRemove(const RemoveEvent& e) override;
        void subscribe(RemovalSubscriber* subscriber);
        void unsubscribe(RemovalSubscriber* subscriber);

        void SapphireModule_initialize()
        {
            // Disable low sensitivity on all attenuverters.
            const int nparams = static_cast<int>(paramInfo.size());
            for (int paramId = 0; paramId < nparams; ++paramId)
                resetAttenuverter(paramId);

            // Clear any voltage-flipping on output ports.
            const int nOutputs = static_cast<int>(outputPortInfo.size());
            for (int outputId = 0; outputId < nOutputs; ++outputId)
                outputPortInfo.at(outputId).flipVoltagePolarity = false;

            enableLimiterWarning = true;

            if (dcRejectQuantity)
                dcRejectQuantity->initialize();

            if (agcLevelQuantity)
                agcLevelQuantity->initialize();

            envelopeFollower.initialize();
        }

        bool isFireDrillOneShot()
        {
            if (fireDrillTrigger)
            {
                fireDrillTrigger = false;
                return true;
            }
            return false;
        }

        bool isEnvelopeFollowerEnabled() const
        {
            return envelopeFollower.enabled;
        }

        void enableEnvelopeFollower()
        {
            envelopeFollower.enabled = true;
        }

        bool duck() const
        {
            return envelopeFollower.duck;
        }

        float cvGetControlValue(int paramId, int attenId, float cv, float minValue = 0, float maxValue = 1)
        {
            float slider = params.at(paramId).getValue();
            // When the attenuverter is set to 100%, and the cv is +5V, we want
            // to swing a slider that is all the way down (minSlider)
            // to act like it is all the way up (maxSlider).
            // Thus we allow the complete range of control for any CV whose
            // range is [-5, +5] volts.
            float attenu = params.at(attenId).getValue();
            const auto& context = paramInfo.at(attenId).context;
            if (isLowSensitive(attenId))
                attenu /= AttenuverterLowSensitivityDenom;
            slider += attenu*(context.adjustVoltage(cv) / 5)*(maxValue - minValue);
            return std::clamp(slider, minValue, maxValue);
        }

        float cvGetVoltPerOctave(int paramId, int attenId, float cv, float minValue, float maxValue)
        {
            // Make it easy for a human to use this control voltage for V/OCT.
            // Just turn the attenuverter to +100%, disable "low sensitivity",
            // and each 1V change in CV is reflected in the return value (within clamping limits).
            float slider = params.at(paramId).getValue();
            float attenu = params.at(attenId).getValue();
            const auto& context = paramInfo.at(attenId).context;
            if (isLowSensitive(attenId))
                attenu /= AttenuverterLowSensitivityDenom;
            slider += attenu * context.adjustVoltage(cv);
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

        void reportChaosMono(int attenId, float chaos)
        {
            SapphireAttenuverterContext& context = paramInfo.at(attenId).context;
            context.chaosVoltage[0] = context.chaosVoltage[1] = chaos;
        }

        void reportChaosStereo(int attenId, float stereoCrossfade, float chaosLeft, float chaosRight)
        {
            SapphireAttenuverterContext& context = paramInfo.at(attenId).context;
            context.chaosVoltage[0] = chaosLeft;
            context.chaosVoltage[1] = LinearMix(stereoCrossfade, chaosLeft, chaosRight);
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

        float getControlValueChaos(int paramId, int attenId, int inputId, float chaos, float minValue = 0, float maxValue = 1, float cvScalar = 1)
        {
            Input& input = inputs.at(inputId);
            float cv = input.isConnected() ? input.getVoltageSum() : chaos;
            return cvGetVoltPerOctave(paramId, attenId, cvScalar * cv, minValue, maxValue);
        }

        void defineAttenuverterId(int attenId, int cvInputId)
        {
            // We need to know which parameter IDs are actually for attenuverter knobs.
            // This is a hook for passing in that information.
            SapphireParamInfo& info = paramInfo.at(attenId);
            info.isAttenuverter = true;
            info.context.inputPortId = cvInputId;
        }

        void attenuverterChaosOptIn(int attenId)
        {
            SapphireParamInfo& info = paramInfo.at(attenId);
            info.context.supportsChaos = true;
        }

        bool isAttenuverter(int paramId) const
        {
            return paramInfo.at(paramId).isAttenuverter;
        }

        SapphireAttenuverterContext* getAttenuverterContext(int attenId)
        {
            return &paramInfo.at(attenId).context;
        }

        bool isLowSensitive(int attenId) const
        {
            return paramInfo.at(attenId).context.lowSensitivityMode;
        }

        void setLowSensitive(int attenId, bool state)
        {
            paramInfo.at(attenId).context.lowSensitivityMode = state;
        }

        void resetAttenuverter(int attenId)
        {
            paramInfo.at(attenId).context.initialize();
        }

        bool isUnipolar(int attenId) const
        {
            return paramInfo.at(attenId).context.unipolar;
        }

        MenuItem* createToggleAllSensitivityMenuItem()
        {
            return createMenuItem(
                "Toggle sensitivity on all attenuverters",
                "",
                [this]{ InvokeAction(new ToggleAllSensitivityAction(this)); }
            );
        }


        MenuItem* createStereoSplitterMenuItem();
        MenuItem* createStereoMergeMenuItem();

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

        static char* formatHex64(char text[17], uint64_t value)
        {
            uint64_t accum = value;
            for (unsigned i = 0; i < 16; ++i)
            {
                unsigned d = accum & 0x0f;
                accum >>= 4;
                text[15-i] = (d >= 10) ? (d - 10 + 'a') : (d + '0');
            }
            text[16] = '\0';
            return text;
        }

        static uint64_t parseHex64(const char *text, uint64_t fallback)
        {
            if (text)
            {
                uint64_t value = 0;
                for (unsigned i = 0; i < 16; ++i)
                {
                    value <<= 4;
                    char c = text[i];
                    if (c >= 'a' && c <= 'f')
                        value |= (c - 'a' + 10);
                    else if (c >= '0' && c <= '9')
                        value |= (c - '0');
                    else
                        return fallback;
                }
                if (text[16] == '\0')
                    return value;
            }
            return fallback;
        }

        static json_t* jsonSeedValue(uint64_t value)
        {
            // The numeric values in json are 64-bit floating point,
            // which is only precise enough to represent 53-bit integers.
            // Sapphire needs to save/restore unsigned 64-bit integers
            // for pseudorandom seeds. We will represent 64-bit integers
            // as strings written in hexadecimal.
            char text[17];    // 2 hex characters per byte, plus null terminator
            formatHex64(text, value);
            return json_string(text);
        }

        static void jsonSaveSeed(json_t* root, const char* key, uint64_t value)
        {
            json_t* jseed = jsonSeedValue(value);
            json_object_set_new(root, key, jseed);
        }

        static uint64_t jsonLoadHex64(json_t* root, const char* key, uint64_t fallback)
        {
            if (json_t* jhex = json_object_get(root, key); json_is_string(jhex))
                if (const char *hex = json_string_value(jhex))
                    return parseHex64(hex, fallback);

            return fallback;
        }

        static uint64_t jsonLoadOrGenerateSeed(json_t* root, const char* key)
        {
            uint64_t seed = jsonLoadHex64(root, key, 0);
            return seed ? seed : rack::random::u64();
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

            // List attenuverters with unipolar clamping enabled.
            json_t* uList = json_array();
            for (int attenId = 0; attenId < nparams; ++attenId)
                if (isUnipolar(attenId))
                    json_array_append(uList, json_integer(attenId));
            json_object_set_new(root, "unipolarAttenuverters", uList);

            // List all attenuverter offsets.
            json_t* adjustList = json_array();
            for (int attenId = 0; attenId < nparams; ++attenId)
                json_array_append(adjustList, json_real(paramInfo.at(attenId).context.adjust));
            json_object_set_new(root, "unipolarOffsetVolts", adjustList);

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

            if (agcLevelQuantity)
            {
                agcLevelQuantity->save(root, "agcLevel");
                json_object_set_new(root, "limiterWarningLight", json_boolean(enableLimiterWarning));
            }

            envelopeFollower.saveJson(root);
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
                paramInfo.at(attenId).context.initialize();

            if (json_t* aList = json_object_get(root, "lowSensitivityAttenuverters"))
            {
                std::size_t listLength = static_cast<int>(json_array_size(aList));
                for (std::size_t listIndex = 0; listIndex < listLength; ++listIndex)
                {
                    json_t *item = json_array_get(aList, listIndex);
                    if (json_is_integer(item))
                    {
                        int attenId = static_cast<int>(json_integer_value(item));
                        if (attenId >= 0 && attenId < nparams)
                            paramInfo.at(attenId).context.lowSensitivityMode = true;
                    }
                }
            }

            if (json_t* uList = json_object_get(root, "unipolarAttenuverters"))
            {
                std::size_t listLength = json_array_size(uList);
                for (std::size_t listIndex = 0; listIndex < listLength; ++listIndex)
                {
                    json_t *item = json_array_get(uList, listIndex);
                    if (json_is_integer(item))
                    {
                        int attenId = static_cast<int>(json_integer_value(item));
                        if (attenId >= 0 && attenId < nparams)
                            paramInfo.at(attenId).context.unipolar = true;
                    }
                }
            }

            if (json_t* oList = json_object_get(root, "voltageFlippedOutputPorts"))
            {
                std::size_t listLength = json_array_size(oList);
                for (std::size_t listIndex = 0; listIndex < listLength; ++listIndex)
                {
                    json_t *item = json_array_get(oList, listIndex);
                    if (json_is_integer(item))
                    {
                        int outputId = static_cast<int>(json_integer_value(item));
                        const int nOutputs = outputPortInfo.size();
                        if (outputId >= 0 && outputId < nOutputs)
                            outputPortInfo.at(outputId).flipVoltagePolarity = true;
                    }
                }
            }

            if (json_t* adjustList = json_object_get(root, "unipolarOffsetVolts"))
            {
                std::size_t listLength = json_array_size(adjustList);
                for (std::size_t listIndex = 0; listIndex < listLength && (int)listIndex < nparams; ++listIndex)
                    if (json_t* item = json_array_get(adjustList, listIndex); json_is_real(item))
                        paramInfo.at(listIndex).context.adjust = json_real_value(item);
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

            // If the JSON is damaged, default to enabling the warning light.
            enableLimiterWarning = !json_is_false(json_object_get(root, "limiterWarningLight"));

            if (agcLevelQuantity)
                agcLevelQuantity->load(root, "agcLevel");

            envelopeFollower.loadJson(root);
        }

        virtual void tryCopySettingsFrom(SapphireModule* other)
        {
        }

        virtual void postInsertFilterHook()
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

        float nextVoltageOrChaosSignal(float& voltage, int inputId, int channel, float chaos)
        {
            Input& input = inputs.at(inputId);
            if (!input.isConnected())
                voltage = chaos;
            else if (channel >= 0 && channel < input.getChannels())
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

        void configStereoButton(int paramId, const std::string& name)
        {
            configParam<StereoButtonQuantity>(paramId, 0, 1, 0, name);
        }

        void configInputStereoButton(int paramId)
        {
            configStereoButton(paramId, "Input stereo split");
        }

        void configOutputStereoButton(int paramId)
        {
            configStereoButton(paramId, "Output stereo merge");
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

        bool isBadOutput(const float outputArray[], int arrayLength) const
        {
            for (int i = 0; i < arrayLength; ++i)
                if (isBadOutput(outputArray[i]))
                    return true;

            return false;
        }

        void beginRecovery(float sampleRateHz)
        {
            redAlert = true;
            limiterRecoveryCountdown = static_cast<int>(sampleRateHz/2);
        }

        static void clearOutput(float outputArray[], int arrayLength)
        {
            for (int c = 0; c < arrayLength; ++c)
                outputArray[c] = 0;
        }

        virtual double getAgcDistortion()
        {
            return 0.0;     // no distortion
        }

        virtual NVGcolor getWarningColor()
        {
            if (!enableLimiterWarning)
                return nvgRGBA(0, 0, 0, 0);     // no warning light

            if (limiterRecoveryCountdown > 0)
            {
                // The module is recovering from non-finite (NAN/infinite) output.
                // Inflict an obnoxiously bright pink OUTPUT knob glow on the user!
                return nvgRGBA(0xff, 0x00, 0xff, 0xb0);
            }

            const double distortion = getAgcDistortion();
            if (distortion <= 0.0)
                return nvgRGBA(0, 0, 0, 0);     // no warning light

            double decibels = 20 * std::log10(1.0 + distortion);
            double scale = std::clamp<double>(decibels/24, 0, 1);

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

        void addLimiterMenuItems(Menu* menu);
        MenuItem* createLimiterWarningLightMenuItem();

        void addAgcLevelQuantity(
            int   paramId,
            float levelMin   =  1.0,
            float levelDef   =  4.0,
            float levelMax   = 10.0,
            float disableMin = 10.1,
            float disableMax = 10.2)
        {
            assert(agcLevelQuantity == nullptr);    // creating more than once would leak memory

            agcLevelQuantity = configParam<AgcLevelQuantity>(
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
        }

        void updateTriggerGroup(
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

            receiver.update(inputVoltage, sampleRateHz);
            setLightBrightness(buttonLightId, receiver.lit());
        }

        void setLightBrightness(int lightId, bool lit)
        {
            if (IsSafeAccess(lights, lightId))
                lights.at(lightId).setBrightness(lit ? 1.0f : 0.06f);
        }

        void updateParamTooltip(int paramId, const std::string& text)
        {
            if (IsSafeAccess(params, paramId))
                if (ParamQuantity* qty = getParamQuantity(paramId))
                    if (qty->name != text)
                        qty->name = text;
        }

        void updateToggleButtonTooltip(int paramId, const char* offText, const char *onText)
        {
            if (IsSafeAccess(params, paramId))
                if (ParamQuantity* qty = getParamQuantity(paramId))
                    if (const char *text = (qty->getValue() < 0.5f) ? offText : onText; strcmp(text, qty->name.c_str()))
                        qty->name = text;
        }

        void updateInputTooltip(int inputId, const std::string& text)
        {
            if (IsSafeAccess(inputInfos, inputId))
                if (PortInfo* info = inputInfos.at(inputId))
                    if (info->name != text)
                        info->name = text;
        }

        float readSample(float normal, Input& inLeft, Input& inRight, int c)
        {
            if (inLeft.isConnected())
            {
                if (inRight.isConnected())
                {
                    // Stereo input
                    if (c == 0)
                        return inLeft.getVoltageSum();
                    if (c == 1)
                        return inRight.getVoltageSum();
                    return 0;
                }

                if (int ncLeft = inLeft.getChannels(); ncLeft > 1)
                {
                    // Polyphonic input
                    if (0 <= c && c < ncLeft)
                        return inLeft.getVoltage(c);
                    return 0;
                }

                // Mono input, so split the signal across both stereo output channels.
                if (c==0 || c==1)
                    return inLeft.getVoltageSum() / 2;
                return 0;
            }
            return normal;
        }

        float readSample(float normal, int inLeftPortId, int inRightPortId, int c)
        {
            Input& inLeft  = inputs.at(inLeftPortId);
            Input& inRight = inputs.at(inRightPortId);
            return readSample(normal, inLeft, inRight, c);
        }

        void addPolyphonicEnvelopeMenuItem(Menu* menu);
        void setPolyphonicEnvelopeOutput(bool state);
        void toggleEnvDuck();

        void updateEnvelope(int outputId, int envGainParamId, float sampleRateHz, int nchannels, const float* sample)
        {
            Output& envOutput = outputs.at(outputId);
            if (envOutput.isConnected())
            {
                const int nc = VcvSafeChannelCount(nchannels);
                const float gain = FourthPower(params.at(envGainParamId).getValue());

                if (envelopeFollower.polyphonicOutput)
                {
                    envOutput.setChannels(nc);
                    for (int c = 0; c < nc; ++c)
                    {
                        float e = envelopeFollower.follower[c].update(sample[c], sampleRateHz);
                        float v = gain * e;
                        float s = envelopeFollower.scaleEnvelope(v, sampleRateHz);
                        envOutput.setVoltage(s, c);
                    }
                }
                else
                {
                    float sum = 0;
                    for (int c = 0; c < nc; ++c)
                        sum += sample[c];

                    float e = envelopeFollower.follower[0].update(sum, sampleRateHz);
                    float v = gain * e;
                    float s = envelopeFollower.scaleEnvelope(v, sampleRateHz);
                    envOutput.setChannels(1);
                    envOutput.setVoltage(s, 0);
                }
            }
        }

        virtual bool shouldDisplayChaosVoltages()
        {
            return false;
        }
    };


    enum class ToggleGroupMode
    {
        Gate,
        Trigger,
        LEN,

        Default = Gate,
    };


    class ToggleGroup
    {
    private:
        SapphireModule* smod = nullptr;
        std::string menuName;
        const char *jsonKey = nullptr;
        int inputId = -1;
        int buttonParamId = -1;
        int buttonLightId = -1;
        GateTriggerReceiver receiver;
        bool portActive = false;

    public:
        ToggleGroupMode mode = ToggleGroupMode::Default;
        bool addPortMenuItems = true;

        ToggleGroup()
        {
            initialize();
        }

        void initialize()
        {
            receiver.initialize();
            mode = ToggleGroupMode::Default;
            portActive = false;
        }

        void config(
            SapphireModule* _smod,
            const std::string& _menuName,
            const char *_jsonKey,
            int _inputId,
            int _buttonParamId,
            int _buttonLightId,
            const std::string& buttonCaption,
            const std::string& inputPrefix)
        {
            smod = _smod;
            menuName = _menuName;
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

            jsonSetEnum(child, "mode", mode);
            jsonSetBool(child, "active", portActive);
        }

        void jsonLoad(json_t* root)
        {
            if (json_t* child = json_object_get(root, jsonKey); json_is_object(child))
            {
                jsonLoadEnum(child, "mode", mode);
                jsonLoadBool(child, "active", portActive);
            }
        }

        bool process()
        {
            if (!smod || inputId<0 || buttonParamId<0 || buttonLightId<0)
                return false;

            Input& input = smod->inputs.at(inputId);
            Param& button = smod->params.at(buttonParamId);
            const bool buttonActive = (button.getValue() > 0);

            receiver.update(input.getVoltageSum());
            switch (mode)
            {
            case ToggleGroupMode::Gate:
            default:
                portActive = receiver.isGateActive();
                break;

            case ToggleGroupMode::Trigger:
                portActive ^= receiver.isTriggerActive();
                break;
            }

            // Allow the button to toggle the gate state, so the gate can be active-low or active-high.
            bool active = portActive ^ buttonActive;
            smod->setLightBrightness(buttonLightId, active);
            return active;
        }

        void addMenuItems(Menu* menu)
        {
            if (addPortMenuItems)
            {
                menu->addChild(createIndexSubmenuItem(
                    menuName + " input port mode",
                    { "Gate", "Trigger" },
                    [=]() { return static_cast<std::size_t>(mode); },
                    [=](size_t value)
                    {
                        const ToggleGroupMode newMode = static_cast<ToggleGroupMode>(value);
                        if (newMode != mode)
                            InvokeAction(new ChangeEnumAction<ToggleGroupMode>(mode, newMode, "toggle gate/port input mode"));
                    }
                ));
            }
        }
    };


    class WarningLightWidget : public LightWidget, public RemovalSubscriber
    {
    private:
        SapphireModule *smod{};

    public:
        explicit WarningLightWidget(SapphireModule *_smod)
            : smod(_smod)
        {
            borderColor = nvgRGBA(0x00, 0x00, 0x00, 0x00);      // don't draw a circular border
            bgColor     = nvgRGBA(0x00, 0x00, 0x00, 0x00);      // don't mess with the knob behind the light
            if (smod)
                smod->subscribe(this);
        }

        void onRemove(const Widget::RemoveEvent& e) override
        {
            if (smod)
                smod->unsubscribe(this);
        }

        void disconnect() override
        {
            smod = nullptr;
        }

        void drawLayer(const DrawArgs& args, int layer) override
        {
            if (layer == 1)
                color = smod ? smod->getWarningColor() : nvgRGBA(0, 0, 0, 0);

            LightWidget::drawLayer(args, layer);
        }
    };


    struct VoltageFlipAction : history::Action
    {
        const int64_t moduleId;
        const int outputId;
        const bool oldValue;

        explicit VoltageFlipAction(const SapphireModule* module, int _outputId)
            : moduleId(module->id)
            , outputId(_outputId)
            , oldValue(module->getVoltageFlipEnabled(_outputId))
        {
            name = "flip voltage polarity";
        }

        void setFlip(bool state)
        {
            if (SapphireModule* module = FindSapphireModule(moduleId))
                module->setVoltageFlipEnabled(outputId, state);
        }

        void undo() override
        {
            setFlip(oldValue);
        }

        void redo() override
        {
            setFlip(!oldValue);
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

        void appendContextMenu(Menu* menu) override
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
                        if (state != module->getVoltageFlipEnabled(outputId))
                            InvokeAction(new VoltageFlipAction(module, outputId));
                    }
                ));
            }
        }
    };


    struct ToggleGroupInputPort : SapphirePort
    {
        ToggleGroup* group = nullptr;

        void appendContextMenu(Menu* menu) override
        {
            SapphirePort::appendContextMenu(menu);
            if (group)
            {
                menu->addChild(new MenuSeparator);
                group->addMenuItems(menu);
            }
        }
    };


    struct EnvelopeOutputPort : SapphirePort
    {
        void appendContextMenu(Menu* menu) override;
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


    inline NVGcolor FadeColor(float fade, float intensity, NVGcolor c0, NVGcolor c1)
    {
        NVGcolor cm;
        cm.a = 1;
        cm.r = intensity * ((1-fade)*c0.r + fade*c1.r);
        cm.g = intensity * ((1-fade)*c0.g + fade*c1.g);
        cm.b = intensity * ((1-fade)*c0.b + fade*c1.b);
        return cm;
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
