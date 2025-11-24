#include "sapphire_chaos_module.hpp"

namespace Sapphire
{
    namespace Chaos
    {
        static Model* PeekModel(Module* module, ExpanderDirection dir)
        {
            if (module)
            {
                Module* nextModule =
                    (dir == ExpanderDirection::Left) ?
                    module->leftExpander.module :
                    module->rightExpander.module;

                if (nextModule)
                    return nextModule->model;
            }
            return nullptr;
        }

        void InsertButton::onButton(const event::Button& e)
        {
            insert_button_base_t::onButton(e);
            if (parentWidget && expanderModel)
            {
                if (e.action == GLFW_RELEASE && e.button == GLFW_MOUSE_BUTTON_LEFT)
                {
                    // Look in the requested insertion direction to see if
                    // the desired model is already present.
                    // If so, suppress adding a redundant instance of that model.
                    Model* peekModel = PeekModel(parentWidget->module, direction);
                    if (expanderModel != peekModel)
                        AddExpander(expanderModel, parentWidget, direction, false);
                }
            }
        }
    }
}
