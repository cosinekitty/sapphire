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
        std::string formula[3]
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

        void updateProgram()
        {
            circuit.resetProgram();
            for (int v = 0; v < 3; ++v)
            {
                BytecodeResult result = circuit.compile(formula[v]);
                if (result.failure())
                {
                    // FIXFIXFIX: format and report the error in the UI.
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
                    editField->changeHandler = [=](std::string text)
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
