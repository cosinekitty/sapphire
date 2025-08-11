#include "sapphire_chaos_module.hpp"
#include "sapphire_prog_chaos.hpp"

namespace Sapphire
{
    // [Don Cross] I copied this menu editor stuff from Venom source code:
    // https://github.com/DaveBenham/VenomModules/blob/f94e4d7d3380b387317746049e4983a278bf99f3/src/plugin.hpp#L99
    // MenuTextField extracted from pachde1 components.hpp
    // Textfield as menu item, originally adapted from SubmarineFree
    struct MenuTextField : ui::TextField
    {
        std::function<void(std::string)> changeHandler;
        std::function<void(std::string)> commitHandler;
        void step() override
        {
            // Keep selected
            APP->event->setSelectedWidget(this);
            TextField::step();
        }
        void setText(const std::string &text)
        {
            this->text = text;
            selectAll();
        }

        void onChange(const ChangeEvent &e) override
        {
            ui::TextField::onChange(e);
            if (changeHandler)
            {
                changeHandler(text);
            }
        }

        void onSelectKey(const event::SelectKey &e) override
        {
            if (e.action == GLFW_PRESS && (e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER))
            {
                if (commitHandler)
                {
                    commitHandler(text);
                }
                ui::MenuOverlay *overlay = getAncestorOfType<ui::MenuOverlay>();
                overlay->requestDelete();
                e.consume(this);
            }
            if (!e.getTarget())
                TextField::onSelectKey(e);
        }
    };


    using ZooModuleBase = Sapphire::Chaos::ChaosModule<ProgOscillator>;
    struct ZooModule : ZooModuleBase
    {
        std::string formula[ProgOscillator::InputCount];

        explicit ZooModule()
            : ZooModuleBase()
        {
            initialLocationFromMemory = true;
            addDilateQuantity();
            addTranslateQuantities();
            ZooModule_initialize();
        }

        void ZooModule_initialize()
        {
            // Reset all parameter mappings.

            // Default to the Rossler attractor.
            formula[0] = "-y-z";
            formula[1] = "x+a*y";
            formula[2] = "b+z*(x-c)";

            circuit.setDilate(0.2);
            dilateQuantity->value = circuit.getDilate();
            dilateQuantity->changed = false;

            circuit.setTranslate(SlopeVector(0, 0, 0));
            xTranslateQuantity->value = 0;
            xTranslateQuantity->changed = false;
            yTranslateQuantity->value = 0;
            yTranslateQuantity->changed = false;
            zTranslateQuantity->value = 0;
            zTranslateQuantity->changed = false;

            circuit.knobMap[0].center = 0.10;
            circuit.knobMap[0].spread = 0.08;
            circuit.knobMap[1].center = 0.10;
            circuit.knobMap[1].spread = 0.08;
            circuit.knobMap[2].center = 14.0;
            circuit.knobMap[2].spread = 10.0;
            circuit.knobMap[3].center = 0.0;
            circuit.knobMap[3].spread = 1.0;

            // Initial position chosen to work well with the Rossler attractor.
            // I obtained this by running the Rossler attractor a long time and picking
            // what looked aesthetically good to me.
            constexpr double xInit = -3.4423733871317674;
            constexpr double yInit = +9.6995732322903141;
            constexpr double zInit = +0.0060546068991647953;
            memory[0] = ChaoticOscillatorState(xInit, yInit, zInit);
            circuit.setState(memory[0]);
            updateProgram();
        }

        void onReset(const ResetEvent& e) override
        {
            ZooModuleBase::onReset(e);
            ZooModule_initialize();
        }

        json_t* dataToJson() override
        {
            json_t* root = ZooModuleBase::dataToJson();

            json_t* program = json_object();
            json_object_set_new(program, "vx", json_string(formula[0].c_str()));
            json_object_set_new(program, "vy", json_string(formula[1].c_str()));
            json_object_set_new(program, "vz", json_string(formula[2].c_str()));
            json_object_set_new(root, "program", program);

            json_object_set_new(root, "dilate", json_real(circuit.getDilate()));

            json_t* jTranslate = json_object();
            SlopeVector t = circuit.getTranslate();
            json_object_set_new(jTranslate, "x", json_real(t.mx));
            json_object_set_new(jTranslate, "y", json_real(t.my));
            json_object_set_new(jTranslate, "z", json_real(t.mz));
            json_object_set_new(root, "translate", jTranslate);

            json_t* jparams = json_array();      // [ {"center":c, "spread":s}, ... ]
            for (int m = 0; m < ProgOscillator::ParamCount; ++m)
            {
                json_t* jmap = json_object();
                json_object_set_new(jmap, "center", json_real(circuit.knobMap[m].center));
                json_object_set_new(jmap, "spread", json_real(circuit.knobMap[m].spread));
                json_array_append(jparams, jmap);
            }
            json_object_set_new(root, "params", jparams);

            return root;
        }

        void dataFromJson(json_t* root) override
        {
            ZooModuleBase::dataFromJson(root);
            if (json_t* program = json_object_get(root, "program"); json_is_object(program))
            {
                loadFormula(program, "vx", formula[0]);
                loadFormula(program, "vy", formula[1]);
                loadFormula(program, "vz", formula[2]);
                updateProgram();
            }

            if (json_t* jdilate = json_object_get(root, "dilate"); json_is_number(jdilate))
                circuit.setDilate(json_real_value(jdilate));

            if (json_t* jTranslate = json_object_get(root, "translate"); json_is_object(jTranslate))
            {
                SlopeVector t = circuit.getTranslate();

                if (json_t* jx = json_object_get(jTranslate, "x"); json_is_number(jx))
                    t.mx = json_real_value(jx);

                if (json_t* jy = json_object_get(jTranslate, "y"); json_is_number(jy))
                    t.my = json_real_value(jy);

                if (json_t* jz = json_object_get(jTranslate, "z"); json_is_number(jz))
                    t.mz = json_real_value(jz);

                circuit.setTranslate(t);
                if (xTranslateQuantity) xTranslateQuantity->value = t.mx;
                if (yTranslateQuantity) yTranslateQuantity->value = t.my;
                if (zTranslateQuantity) zTranslateQuantity->value = t.mz;
            }

            if (json_t* jparams = json_object_get(root, "params"); json_is_array(jparams))
            {
                if (json_array_size(jparams) == ProgOscillator::ParamCount)
                {
                    for (int m = 0; m < ProgOscillator::ParamCount; ++m)
                    {
                        if (json_t* jmap = json_array_get(jparams, m); json_is_object(jmap))
                        {
                            if (json_t* jcenter = json_object_get(jmap, "center"); json_is_number(jcenter))
                                circuit.knobMap[m].center = json_real_value(jcenter);

                            if (json_t* jspread = json_object_get(jmap, "spread"); json_is_number(jspread))
                                circuit.knobMap[m].spread = json_real_value(jspread);
                        }
                    }
                }
            }
        }

        void loadFormula(json_t* program, const char *key, std::string& text)
        {
            if (json_t* j = json_object_get(program, key); json_is_string(j))
                text = std::string(json_string_value(j));
        }

        void updateProgram()
        {
            circuit.resetProgram();
            for (int v = 0; v < 3; ++v)
            {
                BytecodeResult result = circuit.compile(formula[v]);
                if (result.failure())
                {
                    WARN("Compiler error for v%c: %s", 'x' + v, result.message.c_str());
                    circuit.resetProgram();
                    break;
                }
            }
            shouldClearTricorder = true;
        }

        void setInfixFormula(int varIndex, std::string infix)
        {
            if (varIndex >= 0 && varIndex < 3)
            {
                formula[varIndex] = infix;
                updateProgram();
            }
        }

        MenuItem* makeFormulaEditor(int varIndex, std::string prefix)
        {
            assert(varIndex>=0 && varIndex<3);
            // Based on this example:
            // https://github.com/DaveBenham/VenomModules/blob/f94e4d7d3380b387317746049e4983a278bf99f3/src/plugin.hpp#L162
            return createSubmenuItem(
                prefix,
                formula[varIndex],
                [=](Menu* menu)
                {
                    auto editField = new MenuTextField;
                    editField->box.size.x = 250;
                    editField->setText(formula[varIndex]);
                    editField->commitHandler = [=](std::string text)
                    {
                        setInfixFormula(varIndex, text);
                    };
                    menu->addChild(editField);
                }
            );
        }

        MenuItem* makeNumericEditor(double* value, std::string caption)
        {
            char buf[100];
            snprintf(buf, sizeof(buf), "%0.16g", *value);
            std::string text(buf);

            return createSubmenuItem(
                caption,
                text,
                [=](Menu* menu)
                {
                    auto editField = new MenuTextField;
                    editField->box.size.x = 250;
                    editField->setText(text);
                    editField->commitHandler = [=](std::string text)
                    {
                        double x;
                        int n = sscanf(text.c_str(), "%lg", &x);
                        if (n==1 && std::isfinite(x))
                            *value = x;
                    };
                    menu->addChild(editField);
                }
            );
        }

        void addParamEditors(Menu* menu, char symbol, KnobParameterMapping& map)
        {
            std::string varname{symbol};
            menu->addChild(new MenuSeparator);
            menu->addChild(makeNumericEditor(&map.center, varname + " center"));
            menu->addChild(makeNumericEditor(&map.spread, varname + " spread"));
        }

        void addFormulaEditorMenuItems(Menu* menu)
        {
            menu->addChild(new MenuSeparator);
            dilateQuantity->value = circuit.getDilate();
            menu->addChild(new Chaos::DilateSlider(dilateQuantity));
            menu->addChild(new Chaos::TranslateSlider(xTranslateQuantity, "x"));
            menu->addChild(new Chaos::TranslateSlider(yTranslateQuantity, "y"));
            menu->addChild(new Chaos::TranslateSlider(zTranslateQuantity, "z"));
            menu->addChild(makeFormulaEditor(0, "vx"));
            menu->addChild(makeFormulaEditor(1, "vy"));
            menu->addChild(makeFormulaEditor(2, "vz"));

            const uint32_t mask = circuit.lowercaseVariables();
            for (int i = 0; i < ProgOscillator::ParamCount; ++i)
                if (uint32_t c = i + 'a'; BytecodeProgram::IsLowercaseVarUsed(mask, c))
                    addParamEditors(menu, c, circuit.knobMap[i]);
        }
    };


    using ZooWidgetBase = Sapphire::Chaos::ChaosWidget<ZooModule>;
    struct ZooWidget : ZooWidgetBase
    {
        ZooModule* zooModule{};

        explicit ZooWidget(ZooModule* module)
            : ZooWidgetBase(module, "zoo", "res/zoo.svg")
            , zooModule(module)
            {}

        void appendContextMenu(Menu* menu) override
        {
            ZooWidgetBase::appendContextMenu(menu);
            if (zooModule)
            {
                zooModule->addFormulaEditorMenuItems(menu);
            }
        }
    };
}

Model* modelSapphireZoo = createSapphireModel<Sapphire::ZooModule, Sapphire::ZooWidget>(
    "Zoo",
    Sapphire::ChaosModuleRoles
);
