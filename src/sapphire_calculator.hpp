#pragma once
#include <vector>
#include <string>
#include <stdexcept>

namespace Sapphire
{
    class CalcError : public std::runtime_error
    {
    public:
        explicit CalcError(const std::string& msg)
            : std::runtime_error(msg)
            {}
    };


    template <typename value_t>
    class Calculator
    {
    private:
        // Lower-case letters are used as variables.
        value_t vars[26]{};       // 0='a', 1='b', ..., 25='z'.

        std::vector<value_t> stack;

    public:

        bool define(char symbol, value_t value)
        {
            if (symbol >= 'a' && symbol <= 'z')
            {
                vars[symbol - 'a'] = value;
                return true;
            }
            return false;   // invalid symbol character
        }

        int stackHeight() const
        {
            return static_cast<int>(stack.size());
        }

        void push(const value_t& value)
        {
            stack.push_back(value);
        }

        value_t pop()
        {
            if (stack.empty())
                throw CalcError("Attempt to pop from empty stack.");

            value_t x = stack.back();
            stack.pop_back();
            return x;
        }

        void execute(char opcode)
        {
            if (!opcode || isspace(opcode))
            {
                // Ignore whitespace characters if present.
            }
            else if (opcode >= 'a' && opcode <= 'z')
            {
                push(vars[opcode - 'a']);
            }
            else switch (opcode)
            {
            case '*':
                push(pop() * pop());
                break;

            default:
                throw CalcError(std::string("Invalid opcode: ") + opcode);
            }
        }

        void execute(const char *postfix)
        {
            if (postfix)
                for (int i = 0; postfix[i]; ++i)
                    execute(postfix[i]);
        }
    };
}
