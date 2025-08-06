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
        std::string formula[3]      // Default to the Rossler attractor
        {
            "-y-z",
            "x+a*y",
            "b+z*(x-c)"
        };

        explicit ZooModule()
            : ZooModuleBase()
        {
            updateProgram();
        }

        json_t* dataToJson() override
        {
            json_t* root = ZooModuleBase::dataToJson();

            json_t* program = json_object();
            json_object_set_new(program, "vx", json_string(formula[0].c_str()));
            json_object_set_new(program, "vy", json_string(formula[1].c_str()));
            json_object_set_new(program, "vz", json_string(formula[2].c_str()));

            json_object_set_new(root, "program", program);
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
                    break;
                }
            }
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

        void addFormulaEditorMenuItems(Menu* menu)
        {
            menu->addChild(new MenuSeparator);
            menu->addChild(makeFormulaEditor(0, "vx"));
            menu->addChild(makeFormulaEditor(1, "vy"));
            menu->addChild(makeFormulaEditor(2, "vz"));
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
